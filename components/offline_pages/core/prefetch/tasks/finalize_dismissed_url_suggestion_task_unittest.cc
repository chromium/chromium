// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/finalize_dismissed_url_suggestion_task.h"

#include <array>
#include <set>
#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class FinalizeDismissedUrlSuggestionTaskTest : public PrefetchTaskTestBase {
 public:
  ~FinalizeDismissedUrlSuggestionTaskTest() override = default;

  PrefetchItem AddItem(PrefetchItemState state) {
    PrefetchItem item = item_generator()->CreateItem(state);
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
    return item;
  }

  const std::array<PrefetchItemState, 6>& finalizable_states() {
    return FinalizeDismissedUrlSuggestionTask::kFinalizableStates;
  }
};

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, StoreFailure) {
  PrefetchItem item = AddItem(PrefetchItemState::RECEIVED_BUNDLE);
  store_util()->SimulateInitializationError();

  RunTask(std::make_unique<FinalizeDismissedUrlSuggestionTask>(store(),
                                                               item.client_id));
}

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, NotFound) {
  PrefetchItem item = AddItem(PrefetchItemState::RECEIVED_BUNDLE);
  RunTask(std::make_unique<FinalizeDismissedUrlSuggestionTask>(
      store(), ClientId("abc", "123")));
  EXPECT_EQ(1, store_util()->CountPrefetchItems());
}

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, Change) {
  // Add an item for each state, and add the FINISHED item to the expectation.
  std::vector<PrefetchItem> items;
  std::set<PrefetchItem> want_items;
  for (const PrefetchItemState state : finalizable_states()) {
    PrefetchItem item = AddItem(state);
    items.push_back(item);
    item.state = PrefetchItemState::FINISHED;
    item.error_code = PrefetchItemErrorCode::SUGGESTION_INVALIDATED;
    want_items.insert(item);
  }
  for (const PrefetchItem& item : items) {
    RunTask(std::make_unique<FinalizeDismissedUrlSuggestionTask>(
        store(), item.client_id));
  }

  std::set<PrefetchItem> final_items;
  store_util()->GetAllItems(&final_items);
  EXPECT_EQ(want_items, final_items);
}

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, NoChange) {
  std::set<PrefetchItem> items;
  // Insert an item for every state that is not affected by this task.
  for (const PrefetchItemState state : GetAllStatesExcept(
           {finalizable_states().begin(), finalizable_states().end()})) {
    items.insert(AddItem(state));
  }

  for (const PrefetchItem& item : items) {
    RunTask(std::make_unique<FinalizeDismissedUrlSuggestionTask>(
        store(), item.client_id));
  }

  std::set<PrefetchItem> final_items;
  store_util()->GetAllItems(&final_items);
  EXPECT_EQ(items, final_items);
}

}  // namespace offline_pages
