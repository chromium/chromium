// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_UTILS_H_

#include <stdint.h>

namespace sql {
class Database;
}  // namespace sql

namespace offline_pages {

// Useful helper functions to implement PrefetchStore related operations.
class PrefetchStoreUtils {
 public:
  // Deletes a prefetch item by its offline ID. Returns whether it was the item
  // was successfully deleted.
  static bool DeletePrefetchItemByOfflineIdSync(sql::Database* db,
                                                int64_t offline_id);
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_UTILS_H_
