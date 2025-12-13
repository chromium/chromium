// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_STORE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_STORE_H_

#include <optional>

#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "sql/database.h"
#include "url/gurl.h"

namespace sql {
class Statement;
}  // namespace sql

namespace optimization_guide {

// Stores page content (in the form of PageContext protos) in an
// SQLite database.
class PageContentStore {
 public:
  explicit PageContentStore(const base::FilePath& db_path);
  ~PageContentStore();

  PageContentStore(const PageContentStore&) = delete;
  PageContentStore& operator=(const PageContentStore&) = delete;

  // Initializes the encryptor. Must be called before any get/add methods.
  void InitWithEncryptor(os_crypt_async::Encryptor encryptor);

  // Adds a new page content entry to the database.
  // `visit_timestamp` is the timestamp at which the URL was visited.
  // `extraction_timestamp` is the time at which page contents were extracted
  // from the page.
  // `tab_id` is platform specific information used to identify tabs. It is
  // integer in iOS and Android. This database should be migrated to a cross
  // platform tab ID implementation once available. If an entry with the same
  // non-null `tab_id` already exists, it will be overwritten.
  bool AddPageContent(const GURL& url,
                      const proto::PageContext& page_context,
                      base::Time visit_timestamp,
                      base::Time extraction_timestamp,
                      std::optional<int64_t> tab_id);

  // Retrieves the page content for a given URL. If multiple, the most recent
  // based on visit timestamp.
  std::optional<proto::PageContext> GetPageContent(const GURL& url);

  // Retrieves the page content for a given tab ID.
  std::optional<proto::PageContext> GetPageContentForTab(int64_t tab_id);

  // Deletes page content, where visit timestamp older than a given `timestamp`.
  bool DeletePageContentOlderThan(base::Time timestamp);

  // Deletes the page content entry for `tab_id`.
  bool DeletePageContentForTab(int64_t tab_id);

  // Deletes the page content entries for `tab_ids`.
  bool DeletePageContentForTabs(const std::set<int64_t>& tab_ids);

  // Deletes all entries.
  bool DeleteAllEntries();

  // Retrieves all tab IDs from the database.
  std::vector<int64_t> GetAllTabIds();

 private:
  bool InitializeDb();

  // The error callback for the database.
  void OnDatabaseError(int extended_error, sql::Statement* stmt);

  std::optional<proto::PageContext> GetPageContentFromStatement(
      sql::Statement* statement);

  base::FilePath db_path_;
  sql::Database db_;

  bool db_initialized_ = false;
  std::optional<os_crypt_async::Encryptor> encryptor_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_STORE_H_
