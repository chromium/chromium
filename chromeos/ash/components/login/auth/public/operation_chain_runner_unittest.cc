// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/operation_chain_runner.h"

#include <memory>

#include "base/test/bind.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(OperationChainRunnerTest, TestEmptyList) {
  RunOperationChain(
      std::make_unique<UserContext>(), {/* no operations */},
      /* success callback */
      base::BindLambdaForTesting(
          [](std::unique_ptr<UserContext> context) { EXPECT_TRUE(context); }),
      /* failure callback */
      base::BindLambdaForTesting([](std::unique_ptr<UserContext> context,
                                    AuthenticationError error) { FAIL(); }));
}

TEST(OperationChainRunnerTest, TestSingleSuccessfulOperation) {
  std::vector<AuthOperation> operations;
  operations.push_back(base::BindLambdaForTesting(
      [](std::unique_ptr<UserContext> context, AuthOperationCallback callback) {
        context->SetAuthSessionIds("session", "broadcast");
        std::move(callback).Run(std::move(context), std::nullopt);
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context) {
        EXPECT_TRUE(context);
        EXPECT_EQ(context->GetAuthSessionId(), "session");
        chain_finished = true;
      }),
      /* failure callback */
      base::BindLambdaForTesting([](std::unique_ptr<UserContext> context,
                                    AuthenticationError error) { FAIL(); }));
  EXPECT_TRUE(chain_finished);
}

TEST(OperationChainRunnerTest, TestSingleFailedOperation) {
  std::vector<AuthOperation> operations;
  operations.push_back(base::BindLambdaForTesting(
      [](std::unique_ptr<UserContext> context, AuthOperationCallback callback) {
        context->SetAuthSessionIds("session", "broadcast");
        std::move(callback).Run(
            std::move(context),
            AuthenticationError{
                cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                    user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND)});
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting(
          [&](std::unique_ptr<UserContext> context) { FAIL(); }),
      /* failure callback */
      base::BindLambdaForTesting(
          [&](std::unique_ptr<UserContext> context, AuthenticationError error) {
            EXPECT_TRUE(context);
            EXPECT_EQ(context->GetAuthSessionId(), "session");
            chain_finished = true;
          }));
  EXPECT_TRUE(chain_finished);
}

TEST(OperationChainRunnerTest, TestSuccesfulSequenceOrdering) {
  std::vector<AuthOperation> operations;
  int order = 0;
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        EXPECT_EQ(order, 0);
        order++;
        std::move(callback).Run(std::move(context), std::nullopt);
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        EXPECT_EQ(order, 1);
        order++;
        context->SetAuthSessionIds("session", "broadcast");
        std::move(callback).Run(std::move(context), std::nullopt);
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        EXPECT_EQ(order, 2);
        order++;
        std::move(callback).Run(std::move(context), std::nullopt);
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context) {
        EXPECT_TRUE(context);
        EXPECT_EQ(context->GetAuthSessionId(), "session");
        chain_finished = true;
      }),
      /* failure callback */
      base::BindLambdaForTesting([](std::unique_ptr<UserContext> context,
                                    AuthenticationError error) { FAIL(); }));
  EXPECT_TRUE(chain_finished);
}

TEST(OperationChainRunnerTest, TestFailedMiddleOperation) {
  std::vector<AuthOperation> operations;
  bool called_first = false;
  bool called_last = false;

  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        called_first = true;
        std::move(callback).Run(std::move(context), std::nullopt);
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        std::move(callback).Run(
            std::move(context),
            AuthenticationError{
                cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                    user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND)});
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        called_last = true;
        std::move(callback).Run(std::move(context), std::nullopt);
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting(
          [](std::unique_ptr<UserContext> context) { FAIL(); }),
      /* failure callback */
      base::BindLambdaForTesting(
          [&](std::unique_ptr<UserContext> context, AuthenticationError error) {
            chain_finished = true;
            EXPECT_TRUE(context);
          }));
  EXPECT_TRUE(chain_finished);
  EXPECT_TRUE(called_first);
  EXPECT_FALSE(called_last);
}

}  // namespace ash
