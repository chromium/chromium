// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/empty_dm_token_retriever.h"

#include <string>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

namespace {

class EmptyDMTokenRetrieverTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EmptyDMTokenRetrieverTest, GetDMToken) {
  test::TestEvent<StatusOr<std::string>> dm_token_retrieved_event;
  EmptyDMTokenRetriever empty_dm_token_retriever;
  empty_dm_token_retriever.RetrieveDMToken(dm_token_retrieved_event.cb());
  const auto dm_token_result = dm_token_retrieved_event.result();
  ASSERT_TRUE(dm_token_result.has_value());
  EXPECT_THAT(dm_token_result.value(), ::testing::IsEmpty());
}

}  // namespace

}  // namespace reporting
