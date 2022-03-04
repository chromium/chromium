// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_URL_TABLE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_URL_TABLE_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "url/gurl.h"

namespace sql {
class Database;
}

namespace segmentation_platform {

// Handles queries to add and remove entries in the URL table in UKM database.
class UkmUrlTable {
 public:
  static constexpr char kTableName[] = "urls";

  explicit UkmUrlTable(sql::Database* db);
  ~UkmUrlTable();

  UkmUrlTable(UkmUrlTable&) = delete;
  UkmUrlTable& operator=(UkmUrlTable&) = delete;

  // Returns an ID for the URL. The ID will be a persistent hash of the `url`.
  static UrlId GenerateUrlId(const GURL& url);

  // Creates the URL table if it doesn't exist.
  bool InitTable();

  // Returns true if `url_id` exists in the database.
  bool IsUrlInTable(UrlId url_id);

  // Writes `url` to database with `url_id`. It is invalid to call this method
  // when `url_id` exists in the database.
  bool WriteUrl(const GURL& url, UrlId url_id);

  // Removes all the URLs in `urls`.
  bool RemoveUrls(const std::vector<UrlId>& urls);

 private:
  raw_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_URL_TABLE_H_
