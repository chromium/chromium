// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_CACHE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_CACHE_H_

#include <algorithm>
#include <concepts>
#include <functional>
#include <list>
#include <utility>

#include "base/time/time.h"

namespace one_time_tokens {

template <typename T, typename GetTimestampFn = std::identity>
concept HasCreationTimestampFn =
    requires(GetTimestampFn getter, const T& item) {
      { std::invoke(getter, item) } -> std::convertible_to<base::TimeTicks>;
    };

template <typename T, typename ProjectionFn = std::identity>
concept ProjectionFnIsEqualsComparable =
    requires(ProjectionFn projection, const T& item) {
      { std::invoke(projection, item) } -> std::equality_comparable;
    };

// A cache for arbitrary items with a fixed expiration time.
template <typename T,
          typename GetTimestampFn = std::identity,
          typename ProjectionFn = std::identity>
  requires HasCreationTimestampFn<T, GetTimestampFn> &&
           ProjectionFnIsEqualsComparable<T, ProjectionFn>
class ExpiringCache {
 public:
  explicit ExpiringCache(base::TimeDelta max_age,
                         GetTimestampFn get_timestamp_fn = GetTimestampFn(),
                         ProjectionFn projection_fn = ProjectionFn())
      : max_age_(max_age),
        get_timestamp_fn_(std::move(get_timestamp_fn)),
        projection_fn_(std::move(projection_fn)) {}
  ExpiringCache(const ExpiringCache&) = delete;
  ExpiringCache& operator=(const ExpiringCache&) = delete;
  ~ExpiringCache() = default;

  // Purges expired items and adds `item` to the cache if it is new.
  // Returns true if an item with the same value did not exist in the cache
  // according to the default equality operator.
  bool PurgeExpiredAndAdd(T item) {
    PurgeExpired();

    if (std::ranges::find(items_, std::invoke(projection_fn_, item),
                          projection_fn_) != items_.end()) {
      return false;
    }

    // Insert the new item while maintaining the sort order (oldest to newest).
    auto it = std::ranges::lower_bound(
        items_, std::invoke(get_timestamp_fn_, item), {}, get_timestamp_fn_);
    items_.insert(it, std::move(item));
    return true;
  }

  // Purges expired items and returns the remaining items.
  const std::list<T>& PurgeExpiredAndGetItems() {
    PurgeExpired();
    return items_;
  }

  // Returns all the items without filtering for expiration.
  const std::list<T>& GetItems() const { return items_; }

  // Purges expired items and returns the remaining items while clearing the
  // cache.
  std::list<T> TakeItems() {
    PurgeExpired();
    return std::exchange(items_, {});
  }

 private:
  // Removes expired items from the cache.
  void PurgeExpired() {
    base::TimeTicks now = base::TimeTicks::Now();
    items_.remove_if([&](const T& item) {
      return now - std::invoke(get_timestamp_fn_, item) > max_age_;
    });
  }

  // Tokens sorted from oldest to newest.
  std::list<T> items_;
  base::TimeDelta max_age_;
  GetTimestampFn get_timestamp_fn_;
  ProjectionFn projection_fn_;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_UTIL_EXPIRING_CACHE_H_
