// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_TEST_UTIL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_TEST_UTIL_H_

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace base {
class ScopedTempDir;
class SimpleTestClock;
}  // namespace base

namespace offline_pages {
struct PrefetchItem;

extern const int kPrefetchStoreCommandFailed;

// Encapsulates the PrefetchStore and provides synchronous operations on the
// store, for test writing convenience.
class PrefetchStoreTestUtil {
 public:
  PrefetchStoreTestUtil();
  ~PrefetchStoreTestUtil();

  // Builds a new store in a temporary directory.
  void BuildStore();
  // Builds the store in memory (no disk storage).
  void BuildStoreInMemory();
  // Releases the ownership of currently controlled store.
  std::unique_ptr<PrefetchStore> ReleaseStore();
  // Deletes the currently held store that was previously built.
  void DeleteStore();

  // Inserts the provided item in store. Returns true if successful.
  bool InsertPrefetchItem(const PrefetchItem& item);

  // Returns the total count of prefetch items in the store.
  int CountPrefetchItems();

  // Gets the item with the provided |offline_id|. Returns null if the item was
  // not found.
  std::unique_ptr<PrefetchItem> GetPrefetchItem(int64_t offline_id);

  // Gets all existing items from the store, inserting them into |all_items|.
  // Returns the number of items found.
  std::size_t GetAllItems(std::set<PrefetchItem>* all_items);
  std::set<PrefetchItem> GetAllItems();

  // Prints a representation of the prefetch store contents.
  std::string ToString();

  // Sets to the ZOMBIE state entries identified by |name_space| and
  // |url|, returning the number of entries found.
  int ZombifyPrefetchItems(const std::string& name_space, const GURL& url);

  // Returns number of rows affected by last SQL statement.
  int LastCommandChangeCount();

  // Gets the prefetch downloader quota value for testing.
  // Quota calculation will use |clock_| as time source.
  int64_t GetPrefetchQuota();

  // Sets the prefetch quota value for testing.
  // Will use |clock_| as time source when writing back quota.
  bool SetPrefetchQuota(int64_t available_quota);

  // Causes the store to behave as if an initialization error occurred.
  void SimulateInitializationError();

  PrefetchStore* store() { return store_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::ScopedTempDir temp_directory_;
  // TODO(jianli): Refactor this class to avoid owning the store.
  std::unique_ptr<PrefetchStore> owned_store_;
  PrefetchStore* store_;
  base::SimpleTestClock clock_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchStoreTestUtil);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_TEST_UTIL_H_
