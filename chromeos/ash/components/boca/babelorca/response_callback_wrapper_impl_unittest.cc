// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/proto/testing_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

using TestingExpectedResponse =
    base::expected<TestingMessage,
                   ResponseCallbackWrapper::TachyonRequestError>;

TEST(ResponseCallbackWrapperImplTest, RespondWithProtoOnSuccess) {
  base::test::TaskEnvironment task_env;
  static constexpr int kProtoFieldValue = 12345;
  base::test::TestFuture<TestingExpectedResponse> test_future;
  ResponseCallbackWrapperImpl<TestingMessage> response_callback(
      test_future.GetCallback<TestingExpectedResponse>());
  TestingMessage message_input;
  message_input.set_int_field(kProtoFieldValue);

  response_callback.Run(message_input.SerializeAsString());
  TestingExpectedResponse result = test_future.Get();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->int_field(), kProtoFieldValue);
}

TEST(ResponseCallbackWrapperImplTest, RespondWithErrorOnFailure) {
  base::test::TaskEnvironment task_env;
  base::test::TestFuture<TestingExpectedResponse> test_future;
  ResponseCallbackWrapperImpl<TestingMessage> response_callback(
      test_future.GetCallback<TestingExpectedResponse>());

  response_callback.Run(base::unexpected(
      ResponseCallbackWrapper::TachyonRequestError::kHttpError));
  TestingExpectedResponse result = test_future.Get();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            ResponseCallbackWrapper::TachyonRequestError::kHttpError);
}

TEST(ResponseCallbackWrapperImplTest, RespondWithErrorOnParseFailure) {
  base::test::TaskEnvironment task_env;
  base::test::TestFuture<TestingExpectedResponse> test_future;
  ResponseCallbackWrapperImpl<TestingMessage> response_callback(
      test_future.GetCallback<TestingExpectedResponse>());

  response_callback.Run("invalid message");
  TestingExpectedResponse result = test_future.Get();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            ResponseCallbackWrapper::TachyonRequestError::kInternalError);
}

}  // namespace
}  // namespace ash::babelorca
