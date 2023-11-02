// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_DATABASE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_DATABASE_HELPER_H_

#include <list>
#include <map>
#include <string>

#include "base/callback.h"
#include "components/browsing_data/content/database_helper.h"
#include "content/public/browser/storage_usage_info.h"

namespace browsing_data {

// Mock for DatabaseHelper.
// Use AddDatabaseSamples() or add directly to response_ list, then call
// Notify().
class MockDatabaseHelper : public DatabaseHelper {
 public:
  explicit MockDatabaseHelper(content::BrowserContext* browser_context);

  MockDatabaseHelper(const MockDatabaseHelper&) = delete;
  MockDatabaseHelper& operator=(const MockDatabaseHelper&) = delete;

  void StartFetching(FetchCallback callback) override;

  void DeleteDatabase(const url::Origin& origin) override;

  // Adds some DatabaseInfo samples.
  void AddDatabaseSamples();

  // Notifies the callback.
  void Notify();

  // Marks all databases as existing.
  void Reset();

  // Returns true if all databases since the last Reset() invokation were
  // deleted.
  bool AllDeleted();

  std::string last_deleted_origin_;

  std::string last_deleted_db_;

 private:
  ~MockDatabaseHelper() override;

  FetchCallback callback_;

  // Stores which databases exist.
  std::map<const std::string, bool> databases_;

  std::list<content::StorageUsageInfo> response_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_DATABASE_HELPER_H_
