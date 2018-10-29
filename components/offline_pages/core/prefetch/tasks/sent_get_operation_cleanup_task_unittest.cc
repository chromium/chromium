// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/sent_get_operation_cleanup_task.h"

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
class TestingPrefetchNetworkRequestFactory
    : public PrefetchNetworkRequestFactory {
 public:
  TestingPrefetchNetworkRequestFactory() {
    ongoing_operation_names_ = std::make_unique<std::set<std::string>>();
  }
  ~TestingPrefetchNetworkRequestFactory() override = default;

  // PrefetchNetworkRequestFactory implementation.
  bool HasOutstandingRequests() const override { return false; }
  void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      PrefetchRequestFinishedCallback callback) override {}
  std::unique_ptr<std::set<std::string>> GetAllUrlsRequested() const override {
    return std::unique_ptr<std::set<std::string>>();
  }
  void MakeGetOperationRequest(
      const std::string& operation_name,
      PrefetchRequestFinishedCallback callback) override {}
  GetOperationRequest* FindGetOperationRequestByName(
      const std::string& operation_name) const override {
    return nullptr;
  }
  std::unique_ptr<std::set<std::string>> GetAllOperationNamesRequested()
      const override {
    return std::make_unique<std::set<std::string>>(*ongoing_operation_names_);
  }

  void AddOngoingOperation(const std::string& operation_name) {
    ongoing_operation_names_->insert(operation_name);
  }

 private:
  std::unique_ptr<std::set<std::string>> ongoing_operation_names_;
};
}  // namespace

class SentGetOperationCleanupTaskTest : public PrefetchTaskTestBase {
 public:
  SentGetOperationCleanupTaskTest() = default;
  ~SentGetOperationCleanupTaskTest() override = default;
};

TEST_F(SentGetOperationCleanupTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  RunTask(&task);
}

TEST_F(SentGetOperationCleanupTaskTest, Retry) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  RunTask(&task);

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::RECEIVED_GCM, store_item->state);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

TEST_F(SentGetOperationCleanupTaskTest, NoRetryForOngoingRequest) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts - 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  std::unique_ptr<TestingPrefetchNetworkRequestFactory> request_factory =
      std::make_unique<TestingPrefetchNetworkRequestFactory>();
  request_factory->AddOngoingOperation(item.operation_name);

  SentGetOperationCleanupTask task(store(), request_factory.get());
  RunTask(&task);

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(item, *store_item);
}

TEST_F(SentGetOperationCleanupTaskTest, ErrorOnMaxAttempts) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  RunTask(&task);

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(PrefetchItemState::FINISHED, store_item->state);
  EXPECT_EQ(PrefetchItemErrorCode::GET_OPERATION_MAX_ATTEMPTS_REACHED,
            store_item->error_code);
  EXPECT_EQ(item.get_operation_attempts, store_item->get_operation_attempts);
}

TEST_F(SentGetOperationCleanupTaskTest, SkipForOngoingRequestWithMaxAttempts) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  item.get_operation_attempts =
      SentGetOperationCleanupTask::kMaxGetOperationAttempts;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  std::unique_ptr<TestingPrefetchNetworkRequestFactory> request_factory =
      std::make_unique<TestingPrefetchNetworkRequestFactory>();
  request_factory->AddOngoingOperation(item.operation_name);

  SentGetOperationCleanupTask task(store(), request_factory.get());
  RunTask(&task);

  std::unique_ptr<PrefetchItem> store_item =
      store_util()->GetPrefetchItem(item.offline_id);
  ASSERT_TRUE(store_item);
  EXPECT_EQ(item, *store_item);
}

TEST_F(SentGetOperationCleanupTaskTest, NoUpdateForOtherStates) {
  std::set<PrefetchItem> items;
  std::vector<PrefetchItemState> all_other_states =
      GetAllStatesExcept({PrefetchItemState::SENT_GET_OPERATION});
  for (const auto& state : all_other_states) {
    PrefetchItem item = item_generator()->CreateItem(state);
    item.get_operation_attempts =
        SentGetOperationCleanupTask::kMaxGetOperationAttempts;
    ASSERT_TRUE(store_util()->InsertPrefetchItem(item));
    items.insert(item);
  }

  SentGetOperationCleanupTask task(store(), prefetch_request_factory());
  RunTask(&task);

  std::set<PrefetchItem> store_items;
  store_util()->GetAllItems(&store_items);
  EXPECT_EQ(items, store_items);
}

}  // namespace offline_pages
