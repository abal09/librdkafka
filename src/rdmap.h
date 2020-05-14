/*
 * librdkafka - The Apache Kafka C/C++ library
 *
 * Copyright (c) 2020 Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RDMAP_H_
#define _RDMAP_H_

/**
 * @name Hash maps.
 *
 * Memory of key and value are allocated by the user but owned by the hash map
 * until elements are deleted or overwritten.
 *
 * The lower-case API provides a generic typeless (void *) hash map while
 * the upper-case API provides a strictly typed hash map implemented as macros
 * on top of the generic API.
 *
 * See rd_map_init(), et.al, for the generic API and RD_MAP_INITIALIZER()
 * for the typed API.
 *
 * @remark Not thread safe.
 */


/**
 * @struct Map element. This is the internal representation
 *         of the element and exposed to the user for iterating over the hash.
 */
typedef struct rd_map_elem_s {
        LIST_ENTRY(rd_map_elem_s) hlink; /**< Hash bucket link */
        LIST_ENTRY(rd_map_elem_s) link;  /**< Iterator link */
        unsigned int hash;               /**< Key hash value */
        const void *key;                 /**< Key (memory owned by map) */
        const void *value;               /**< Value (memory owned by map) */
} rd_map_elem_t;


/**
 * @struct Hash buckets (internal use).
 */
struct rd_map_buckets {
        LIST_HEAD(, rd_map_elem_s) *p; /**< Hash buckets array */
        int cnt;                       /**< Bucket count */
};


/**
 * @struct Hash map.
 */
typedef struct rd_map_s {
        struct rd_map_buckets rmap_buckets;    /**< Hash buckets */
        int rmap_cnt;                          /**< Element count */

        LIST_HEAD(, rd_map_elem_s) rmap_iter;  /**< Element list for iterating
                                                *   over all elements. */

        int (*rmap_cmp) (const void *a, const void *b); /**< Key comparator */
        unsigned int (*rmap_hash) (const void *key);   /**< Key hash function */
        void (*rmap_destroy_key) (void *key);     /**< Optional key free */
        void (*rmap_destroy_value) (void *value); /**< Optional value free */

        void *rmap_opaque;
} rd_map_t;




/**
 * @brief Set/overwrite value in map.
 *
 * If an existing entry with the same key already exists its key and value
 * will be freed with the destroy_key and destroy_value functions
 * passed to rd_map_init().
 *
 * The map assumes memory ownership of both the \p key and \p value and will
 * use the destroy_key and destroy_value functions (if set) to free
 * the key and value memory when the map is destroyed or element removed.
 */
void rd_map_set (rd_map_t *rmap, void *key, void *value);


/**
 * @brief Look up \p key in the map and return its value, or NULL
 *        if \p key was not found.
 *
 * The returned memory is still owned by the map.
 */
void *rd_map_get (rd_map_t *rmap, const void *key);


/**
 * @brief Look up \p key in the map and return its element node object,
 *        or create a new element node object with the given key.
 *
 * Use this to implement something akin to Python's defaultdict.
 */
rd_map_elem_t *rd_map_get_elem (rd_map_t *rmap, const void *key);


/**
 * @brief Delete \p key from the map, if it exists.
 *
 * The destroy_key and destroy_value functions (if set) will be used
 * to free the key and value memory.
 */
void rd_map_delete (rd_map_t *rmap, const void *key);


/**
 * @returns the current number of elements in the map.
 */
size_t rd_map_cnt (rd_map_t *rmap);



/**
 * @brief Iterate over all elements in the map.
 *
 * @warning The map MUST NOT be modified during the loop.
 *
 * @remark This is part of the untyped generic API.
 */
#define RD_MAP_FOREACH_ELEM(ELEM,RMAP)             \
        for (rd_map_iter_begin((RMAP), &(ELEM)) ;  \
             rd_map_iter(&(ELEM)) ;                \
             rd_map_iter_next(&(ELEM)))


/**
 * @brief Begin iterating \p rmap, first element is set in \p *elem.
 */
void rd_map_iter_begin (const rd_map_t *rmap, rd_map_elem_t **elem);

/**
 * @returns 1 if \p *elem is a valid iteration element, else 0.
 */
static RD_INLINE RD_UNUSED
int rd_map_iter (rd_map_elem_t **elem) {
        return *elem != NULL;
}

/**
 * @brief Advances the iteration to the next element.
 */
static RD_INLINE RD_UNUSED
void rd_map_iter_next (rd_map_elem_t **elem) {
        *elem = LIST_NEXT(*elem, link);
}


/**
 * @brief Initialize a map that is expected to hold \p expected_cnt elements.
 *
 * @param expected_cnt Expected number of elements in the map,
 *                     this is used to select a suitable bucket count.
 *                     Passing a value of 0 will set the bucket count
 *                     to a reasonable default.
 * @param cmp Key comparator that must return 0 if the two keys match.
 * @param hash Key hashing function that is used to map a key to a bucket.
 *             It must return an integer hash >= 0 of the key.
 * @param destroy_key (Optional) When an element is deleted or overwritten
 *                    this function will be used to free the key memory.
 * @param destroy_value (Optional) When an element is deleted or overwritten
 *                      this function will be used to free the value memory.
 *
 * Destroy the map with rd_map_destroy()
 *
 * @remarks The map is not thread-safe.
 */
void rd_map_init (rd_map_t *rmap, size_t expected_cnt,
                  int (*cmp) (const void *a, const void *b),
                  unsigned int (*hash) (const void *key),
                  void (*destroy_key) (void *key),
                  void (*destroy_value) (void *value));


/**
 * @brief Internal use
 */
struct rd_map_buckets rd_map_alloc_buckets (size_t expected_cnt);


/**
 * @brief Free all elements in the map and free all memory associated
 *        with the map, but not the rd_map_t itself.
 *
 * The map is unusable after this call but can be re-initialized using
 * rd_map_init().
 */
void rd_map_destroy (rd_map_t *rmap);


/**
 * @brief String comparator for (const char *) keys.
 */
int rd_map_str_cmp (const void *a, const void *b);


/**
 * @brief String hash function (djb2) for (const char *) keys.
 */
unsigned int rd_map_str_hash (const void *a);





/**
 * @name Typed hash maps.
 *
 * Typed hash maps provides a type-safe layer on top of the standard hash maps.
 */
/**
 * @brief Define a typed map type which can later be used with
 *        RD_MAP_INITIALIZER() and typed RD_MAP_*() API.
 */
#define RD_MAP_TYPE(TYPE_NAME,KEY_TYPE,VALUE_TYPE) \
        struct TYPE_NAME {                         \
                rd_map_t rmap;                     \
                KEY_TYPE key;                      \
                VALUE_TYPE value;                  \
                rd_map_elem_t *elem;               \
        }

/**
 * @brief Initialize a typed hash map. The left hand side variable must be
1 *        a typed hash map defined by RD_MAP_TYPE().
 *
 * The typed hash map is a macro layer on top of the rd_map_t implementation
 * that provides type safety.
 * The methods are the same as the underlying implementation but in all caps
 * (to indicate their macro use), e.g., RD_MAP_SET() is the typed version
 * of rd_map_set().
 *
 * @param EXPECTED_CNT Expected number of elements in hash.
 * @param KEY_TYPE The type of the hash key.
 * @param VALUE_TYPE The type of the hash value.
 * @param CMP Comparator function for the key.
 * @param HASH Hash function for the key.
 * @param DESTROY_KEY Destructor for the key type.
 * @param DESTROY_VALUE Destructor for the value type.
 *
 * @sa rd_map_init()
 */
#define RD_MAP_INITIALIZER(EXPECTED_CNT,CMP,HASH,DESTROY_KEY,DESTROY_VALUE) \
        {                                                               \
                .rmap = {                                               \
                        .rmap_buckets = rd_map_alloc_buckets(EXPECTED_CNT), \
                        .rmap_cmp = CMP,                                \
                        .rmap_hash = HASH,                              \
                        .rmap_destroy_key = DESTROY_KEY,                \
                        .rmap_destroy_value = DESTROY_VALUE             \
                }                                                       \
        }


/**
 * @brief Initialize a locally-defined typed hash map.
 *        This hash map can only be used in the current scope/function
 *        as its type is private to this initializement.
 *
 * @param RMAP Hash map variable name.
 *
 * For the other parameters, see RD_MAP_INITIALIZER().
 *
 * @sa RD_MAP_INITIALIZER()
 */
#define RD_MAP_LOCAL_INITIALIZER(RMAP, EXPECTED_CNT,                    \
                                 KEY_TYPE, VALUE_TYPE,                  \
                                 CMP, HASH, DESTROY_KEY, DESTROY_VALUE) \
        struct {                                                        \
                rd_map_t  rmap;                                         \
                KEY_TYPE key;                                           \
                VALUE_TYPE value;                                       \
                rd_map_elem_t *elem;                                    \
        } RMAP = RD_MAP_INITIALIZER(EXPECTED_CNT,CMP,HASH,              \
                                    DESTROY_KEY,DESTROY_VALUE)

/**
 * @brief Typed hash map: Set key/value in map.
 *
 * @sa rd_map_set()
 */
#define RD_MAP_SET(RMAP,KEY,VALUE) do {                                 \
                  (RMAP)->key = KEY;                                    \
                  (RMAP)->value = VALUE;                                \
                  rd_map_set(&(RMAP)->rmap,                             \
                             (void *)(RMAP)->key,                       \
                             (void *)(RMAP)->value);                    \
          } while (0)

/**
 * @brief Typed hash map: Get value for key.
 *
 * @sa rd_map_get()
 */
#define RD_MAP_GET(RMAP,KEY)                                            \
        ((RMAP)->key = (KEY),                                           \
         (RMAP)->value = rd_map_get(&(RMAP)->rmap, (RMAP)->key),        \
         (RMAP)->value)


/**
 * @brief Typed hash map: Delete element by key.
 *
 * The destroy_key and destroy_value functions (if set) will be used
 * to free the key and value memory.
 *
 * @sa rd_map_delete()
 */
#define RD_MAP_DELETE(RMAP,KEY)                                         \
        ((RMAP)->key = (KEY),                                           \
         rd_map_delete(&(RMAP)->rmap, (RMAP)->key))                     \


/**
 * @brief Typed hash map: Destroy hash map.
 *
 * @sa rd_map_destroy()
 */
#define RD_MAP_DESTROY(RMAP)  rd_map_destroy(&(RMAP)->rmap)


/**
 * @brief Typed hash map: Iterate over all elements in the map.
 *
 * @warning The map MUST NOT be modified during the loop.
 */
#define RD_MAP_FOREACH(K,V,RMAP)                                        \
        for (rd_map_iter_begin(&(RMAP)->rmap, &(RMAP)->elem) ;          \
             rd_map_iter(&(RMAP)->elem) &&                              \
                     ((RMAP)->key = (void *)(RMAP)->elem->key,          \
                      (K) = (RMAP)->key,                                \
                      (RMAP)->value = (void *)(RMAP)->elem->value,      \
                      (V) = (RMAP)->value) ;                            \
             rd_map_iter_next(&(RMAP)->elem))





#endif /* _RDMAP_H_ */
