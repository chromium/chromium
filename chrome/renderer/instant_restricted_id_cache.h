// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_INSTANT_RESTRICTED_ID_CACHE_H_
#define CHROME_RENDERER_INSTANT_RESTRICTED_ID_CACHE_H_

#include <stddef.h>

#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/lru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/numerics/wrapping_math.h"
#include "chrome/common/search/instant_types.h"

// In InstantExtended, iframes are used to display objects which can only be
// referenced by the Instant page using an ID (restricted ID). These IDs need to
// be unique and cached for a while so that the SearchBox API can fetch the
// object info based on the ID when required by the Instant page. The reason to
// use a cache of N items as against just the last set of results is that there
// may be race conditions - e.g. the user clicks on a result being shown but the
// result set has internally changed but not yet been displayed.
//
// The cache can be used in two modes:
//
// 1. To store items and assign restricted IDs to them. The cache will store
//    a max of |max_cache_size_| items and assign them unique IDs.
//
// 2. To store items that already have restricted IDs assigned to them (e.g.
//    from another instance of the cache). The cache will then not generate IDs
//    and does not make any guarantees of the uniqueness of the IDs. If multiple
//    items are inserted with the same ID, the cache will return the last
//    inserted item in GetItemWithRestrictedID() call.

// T needs to be copyable.
template <typename T>
class InstantRestrictedIDCache {
 public:
  typedef std::pair<InstantRestrictedID, T> ItemIDPair;
  typedef std::vector<T> ItemVector;
  typedef std::vector<ItemIDPair> ItemIDVector;

  explicit InstantRestrictedIDCache(size_t max_cache_size);

  InstantRestrictedIDCache(const InstantRestrictedIDCache&) = delete;
  InstantRestrictedIDCache& operator=(const InstantRestrictedIDCache&) = delete;

  ~InstantRestrictedIDCache();

  // Adds items to the cache, assigning restricted IDs in the process. May
  // delete older items from the cache. |items.size()| has to be less than max
  // cache size.
  void AddItems(const ItemVector& items);

  // Adds items to the cache using the supplied restricted IDs. May delete
  // older items from the cache. No two entries in |items| should have the same
  // InstantRestrictedID. |items.size()| has to be less than max cache size.
  void AddItemsWithRestrictedID(const ItemIDVector& items);

  // Returns the last set of items added to the cache either via AddItems() or
  // AddItemsWithRestrictedID().
  void GetCurrentItems(ItemIDVector* items) const;

  // Returns true if the |restricted_id| is present in the cache and if so,
  // returns a copy of the item.
  bool GetItemWithRestrictedID(InstantRestrictedID restricted_id,
                               T* item) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(InstantRestrictedIDCacheTest, AutoIDGeneration);
  FRIEND_TEST_ALL_PREFIXES(InstantRestrictedIDCacheTest, CrazyIDGeneration);
  FRIEND_TEST_ALL_PREFIXES(InstantRestrictedIDCacheTest, ManualIDGeneration);
  FRIEND_TEST_ALL_PREFIXES(InstantRestrictedIDCacheTest, MixIDGeneration);
  FRIEND_TEST_ALL_PREFIXES(InstantRestrictedIDCacheTest, AddEmptySet);
  FRIEND_TEST_ALL_PREFIXES(InstantRestrictedIDCacheTest,
                           AddItemsWithRestrictedID);

  typedef base::LRUCache<InstantRestrictedID, T> CacheImpl;

  mutable CacheImpl cache_;
  typename CacheImpl::reverse_iterator last_add_start_;
  InstantRestrictedID last_restricted_id_;
};

template <typename T>
InstantRestrictedIDCache<T>::InstantRestrictedIDCache(size_t max_cache_size)
    : cache_(max_cache_size),
      last_add_start_(cache_.rend()),
      last_restricted_id_(0) {
  DCHECK(max_cache_size);
}

template <typename T>
InstantRestrictedIDCache<T>::~InstantRestrictedIDCache() {
}

template <typename T>
void InstantRestrictedIDCache<T>::AddItems(const ItemVector& items) {
  DCHECK_LE(items.size(), cache_.max_size());

  if (items.empty()) {
    last_add_start_ = cache_.rend();
    return;
  }

  for (size_t i = 0; i < items.size(); ++i) {
    InstantRestrictedID id = base::WrappingAdd(last_restricted_id_, 1);
    last_restricted_id_ = id;
    cache_.Put(id, items[i]);
    if (i == 0)
      last_add_start_ = --cache_.rend();
  }
}

template <typename T>
void InstantRestrictedIDCache<T>::AddItemsWithRestrictedID(
    const ItemIDVector& items) {
  DCHECK_LE(items.size(), cache_.max_size());

  if (items.empty()) {
    last_add_start_ = cache_.rend();
    return;
  }

  std::set<InstantRestrictedID> ids_added;
  for (size_t i = 0; i < items.size(); ++i) {
    const ItemIDPair& item_id = items[i];

    DCHECK(ids_added.find(item_id.first) == ids_added.end());
    ids_added.insert(item_id.first);

    cache_.Put(item_id.first, item_id.second);
    last_restricted_id_ = std::max(item_id.first, last_restricted_id_);
  }

  // cache_.Put() can invalidate the iterator |last_add_start_| is pointing to.
  // Therefore, update |last_add_start_| after adding all the items to the
  // |cache_|.
  last_add_start_ = cache_.rend();
  for (size_t i = 0; i < items.size(); ++i)
    --last_add_start_;
}

template <typename T>
void InstantRestrictedIDCache<T>::GetCurrentItems(ItemIDVector* items) const {
  items->clear();

  for (typename CacheImpl::reverse_iterator it = last_add_start_;
       it != cache_.rend(); ++it) {
    items->push_back(std::make_pair(it->first, it->second));
  }
}

template <typename T>
bool InstantRestrictedIDCache<T>::GetItemWithRestrictedID(
    InstantRestrictedID restricted_id,
    T* item) const {
  DCHECK(item);

  typename CacheImpl::const_iterator cache_it = cache_.Peek(restricted_id);
  if (cache_it == cache_.end())
    return false;
  *item = cache_it->second;
  return true;
}

#endif  // CHROME_RENDERER_INSTANT_RESTRICTED_ID_CACHE_H_
