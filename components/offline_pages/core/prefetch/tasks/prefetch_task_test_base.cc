// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"

#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/task/task_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

// static
constexpr std::array<PrefetchItemState, 11>
    PrefetchTaskTestBase::kOrderedPrefetchItemStates;

PrefetchTaskTestBase::PrefetchTaskTestBase()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      prefs_(std::make_unique<TestingPrefServiceSimple>()),
      prefetch_request_factory_(test_shared_url_loader_factory_, prefs()) {}

PrefetchTaskTestBase::~PrefetchTaskTestBase() = default;

void PrefetchTaskTestBase::SetUp() {
  TaskTestBase::SetUp();
  prefetch_prefs::RegisterPrefs(prefs()->registry());
  store_test_util_.BuildStoreInMemory();
}

void PrefetchTaskTestBase::TearDown() {
  store_test_util_.DeleteStore();
  TaskTestBase::TearDown();
}

// static
std::vector<PrefetchItemState> PrefetchTaskTestBase::GetAllStatesExcept(
    std::set<PrefetchItemState> states_to_exclude) {
  std::vector<PrefetchItemState> selected_states;
  for (const PrefetchItemState state : kOrderedPrefetchItemStates) {
    if (states_to_exclude.count(state) == 0)
      selected_states.push_back(state);
  }
  CHECK_EQ(selected_states.size(),
           kOrderedPrefetchItemStates.size() - states_to_exclude.size());
  return selected_states;
}

int64_t PrefetchTaskTestBase::InsertPrefetchItemInStateWithOperation(
    std::string operation_name,
    PrefetchItemState state) {
  PrefetchItem item;
  item.state = state;
  item.offline_id = store_utils::GenerateOfflineId();
  std::string offline_id_string = std::to_string(item.offline_id);
  item.url = GURL("http://www.example.com/?id=" + offline_id_string);
  item.operation_name = operation_name;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
  return item.offline_id;
}

// static
std::set<PrefetchItem> PrefetchTaskTestBase::FilterByState(
    const std::set<PrefetchItem>& items,
    PrefetchItemState state) {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.state == state)
      result.insert(item);
  }
  return result;
}

network::TestURLLoaderFactory::PendingRequest*
PrefetchTaskTestBase::GetPendingRequest(size_t index) {
  return test_url_loader_factory_.GetPendingRequest(index);
}

}  // namespace offline_pages
