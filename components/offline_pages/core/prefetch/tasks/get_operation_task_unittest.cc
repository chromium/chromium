// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/get_operation_task.h"

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/task/task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoAll;
using testing::HasSubstr;
using testing::SaveArg;
using testing::_;

namespace offline_pages {
namespace {
const char kOperationName[] = "an_operation";
const char kOtherOperationName[] = "an_operation";
const char kOperationShouldNotBeRequested[] = "Operation Not Found";
}  // namespace

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class GetOperationTaskTest : public PrefetchTaskTestBase {
 public:
  GetOperationTaskTest() = default;
  ~GetOperationTaskTest() override = default;
};

TEST_F(GetOperationTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();
  base::MockCallback<PrefetchRequestFinishedCallback> callback;

  RunTask(std::make_unique<GetOperationTask>(
      store(), prefetch_request_factory(), callback.Get()));
}

TEST_F(GetOperationTaskTest, NormalOperationTask) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  int64_t id = InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::RECEIVED_GCM);
  ASSERT_NE(nullptr, store_util()->GetPrefetchItem(id));

  RunTask(std::make_unique<GetOperationTask>(
      store(), prefetch_request_factory(), callback.Get()));

  EXPECT_NE(nullptr, prefetch_request_factory()->FindGetOperationRequestByName(
                         kOperationName));
  std::string path = GetPendingRequest(0 /*index*/)->request.url.path();
  EXPECT_THAT(path, HasSubstr(kOperationName));
  EXPECT_EQ(1, store_util()->LastCommandChangeCount());
  ASSERT_NE(nullptr, store_util()->GetPrefetchItem(id));
  EXPECT_EQ(1, store_util()->GetPrefetchItem(id)->get_operation_attempts);
}

TEST_F(GetOperationTaskTest, NotMatchingEntries) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  // List all states that are not affected by the GetOperationTask.
  std::vector<PrefetchItemState> states = GetAllStatesExcept(
      {PrefetchItemState::SENT_GET_OPERATION, PrefetchItemState::RECEIVED_GCM});
  std::vector<int64_t> entries;
  for (auto& state : states) {
    entries.push_back(
        InsertPrefetchItemInStateWithOperation(kOperationName, state));
  }

  RunTask(std::make_unique<GetOperationTask>(
      store(), prefetch_request_factory(), callback.Get()));

  EXPECT_EQ(nullptr, prefetch_request_factory()->FindGetOperationRequestByName(
                         kOperationName));
  for (int64_t id : entries) {
    EXPECT_NE(PrefetchItemState::SENT_GET_OPERATION,
              store_util()->GetPrefetchItem(id)->state);
    // No attempts should be recorded.
    EXPECT_GT(1, store_util()->GetPrefetchItem(id)->get_operation_attempts);
  }
}

TEST_F(GetOperationTaskTest, TwoOperations) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  int64_t item1 = InsertPrefetchItemInStateWithOperation(
      kOperationName, PrefetchItemState::RECEIVED_GCM);

  int64_t item2 = InsertPrefetchItemInStateWithOperation(
      kOtherOperationName, PrefetchItemState::RECEIVED_GCM);

  // One should not be fetched.
  int64_t unused_item = InsertPrefetchItemInStateWithOperation(
      kOperationShouldNotBeRequested, PrefetchItemState::SENT_GET_OPERATION);

  RunTask(std::make_unique<GetOperationTask>(
      store(), prefetch_request_factory(), callback.Get()));

  EXPECT_NE(nullptr, prefetch_request_factory()->FindGetOperationRequestByName(
                         kOperationName));
  EXPECT_NE(nullptr, prefetch_request_factory()->FindGetOperationRequestByName(
                         kOtherOperationName));
  EXPECT_EQ(1, store_util()->GetPrefetchItem(item1)->get_operation_attempts);
  EXPECT_EQ(1, store_util()->GetPrefetchItem(item2)->get_operation_attempts);

  // The one with no entries in RECEIVED_GCM state should not be requested.
  EXPECT_EQ(nullptr, prefetch_request_factory()->FindGetOperationRequestByName(
                         kOperationShouldNotBeRequested));
  // No attempts should be recorded.
  EXPECT_GT(1,
            store_util()->GetPrefetchItem(unused_item)->get_operation_attempts);
}

}  // namespace offline_pages
