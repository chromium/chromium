// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_SCHEMA_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_SCHEMA_H_

#include <string>

namespace sql {
class Database;
}  // namespace sql

namespace offline_pages {

// Maintains the schema of the prefetch database, ensuring creation and upgrades
// from any and all previous database versions to the latest.
class PrefetchStoreSchema {
 public:
  // See version_schemas for a history of schema versions.
  static constexpr int kCurrentVersion = 3;
  static constexpr int kCompatibleVersion = 1;
  static_assert(kCurrentVersion >= kCompatibleVersion,
                "compatible version shouldn't be greater than the current one");

  // Creates or upgrade the database schema as needed from information stored in
  // a metadata table. Returns |true| if the database is ready to be used,
  // |false| if creation or upgrades failed.
  static bool CreateOrUpgradeIfNeeded(sql::Database* db);

  // Returns the current items table creation SQL command for test usage.
  static std::string GetItemTableCreationSqlForTesting();
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_SCHEMA_H_
