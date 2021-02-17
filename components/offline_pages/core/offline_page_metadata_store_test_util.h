// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_TEST_UTIL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"

namespace base {
class ScopedTempDir;
class SimpleTestClock;
}  // namespace base

namespace offline_pages {

// Encapsulates the OfflinePageMetadataStore and provides synchronous
// operations on the store, for test writing convenience.
class OfflinePageMetadataStoreTestUtil {
 public:
  OfflinePageMetadataStoreTestUtil();
  ~OfflinePageMetadataStoreTestUtil();

  // Builds a new store in a temporary directory.
  void BuildStore();
  // Builds the store in memory (no disk storage).
  void BuildStoreInMemory();
  // Releases the ownership of currently controlled store. But still keeps a raw
  // pointer to the previously owned store in |store_ptr|, until the next time
  // BuildStore*() is called.
  std::unique_ptr<OfflinePageMetadataStore> ReleaseStore();
  // Deletes the currently held store that was previously built.
  void DeleteStore();

  // Inserts an offline page item into the store.
  void InsertItem(const OfflinePageItem& page_to_insert);

  // Gets the total number of pages in the store.
  int64_t GetPageCount();

  // Gets offline page by offline_id.
  std::unique_ptr<OfflinePageItem> GetPageByOfflineId(int64_t offline_id);

  OfflinePageMetadataStore* store() { return store_ptr_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::ScopedTempDir temp_directory_;
  // TODO(romax): Refactor the test util along with the similar one used in
  // Prefetching, to remove the ownership to the store. And clean up related
  // usage of |store_ptr_|.
  std::unique_ptr<OfflinePageMetadataStore> store_;
  OfflinePageMetadataStore* store_ptr_;
  base::SimpleTestClock clock_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMetadataStoreTestUtil);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_TEST_UTIL_H_
