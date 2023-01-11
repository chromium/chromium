// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_INDEXED_DB_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_INDEXED_DB_HELPER_H_

#include <list>
#include <map>

#include "base/functional/callback.h"
#include "components/browsing_data/content/indexed_db_helper.h"

namespace blink {
class StorageKey;
}

namespace content {
class BrowserContext;
}

namespace browsing_data {

// Mock for IndexedDBHelper.
// Use AddIndexedDBSamples() or add directly to response_ list, then
// call Notify().
class MockIndexedDBHelper : public IndexedDBHelper {
 public:
  explicit MockIndexedDBHelper(content::BrowserContext* browser_context);

  MockIndexedDBHelper(const MockIndexedDBHelper&) = delete;
  MockIndexedDBHelper& operator=(const MockIndexedDBHelper&) = delete;

  // Adds some StorageUsageInfo samples.
  void AddIndexedDBSamples();

  // Notifies the callback.
  void Notify();

  // Marks all indexed db files as existing.
  void Reset();

  // Returns true if all indexed db files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  // IndexedDBHelper.
  void StartFetching(FetchCallback callback) override;
  void DeleteIndexedDB(const blink::StorageKey& storage_key,
                       base::OnceCallback<void(bool)> callback) override;

 private:
  ~MockIndexedDBHelper() override;

  FetchCallback callback_;
  std::map<blink::StorageKey, bool> storage_keys_;
  std::list<content::StorageUsageInfo> response_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_INDEXED_DB_HELPER_H_
