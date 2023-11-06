// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_session_storage_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::_;
using testing::Eq;

class AuthSessionStorageImplTest : public ::testing::Test {
 protected:
  AuthSessionStorageImplTest() {
    storage_ = std::make_unique<AuthSessionStorageImpl>(&mock_udac_);
  }
  base::test::SingleThreadTaskEnvironment task_environment;
  ash::MockUserDataAuthClient mock_udac_;
  std::unique_ptr<AuthSessionStorageImpl> storage_;
};

TEST_F(AuthSessionStorageImplTest, Basic) {
  std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
  context->SetAuthSessionIds("some-id", "broadcast");

  // Store UserContext;

  AuthProofToken token = storage_->Store(std::move(context));
  ASSERT_TRUE(storage_->IsValid(token));

  // Borrow context, token is still valid.
  context = storage_->BorrowForTests(FROM_HERE, token);
  ASSERT_TRUE(storage_->IsValid(token));

  // Return context, token still valid
  storage_->Return(token, std::move(context));
  ASSERT_TRUE(storage_->IsValid(token));

  // Do not invoke callback, token should be considered invalid anyway.
  EXPECT_CALL(mock_udac_, InvalidateAuthSession(_, _));
  storage_->Invalidate(token, base::DoNothing());

  ASSERT_FALSE(storage_->IsValid(token));
}

TEST_F(AuthSessionStorageImplTest, InvalidateOnReturn) {
  std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
  context->SetAuthSessionIds("some-id", "broadcast");

  // Store UserContext;
  AuthProofToken token = storage_->Store(std::move(context));
  ASSERT_TRUE(storage_->IsValid(token));
  // Borrow context, token is still valid.
  context = storage_->BorrowForTests(FROM_HERE, token);
  ASSERT_TRUE(storage_->IsValid(token));

  // Do not expect to have any calls before context is returned.
  EXPECT_CALL(mock_udac_, InvalidateAuthSession(_, _)).Times(0);

  storage_->Invalidate(token, base::DoNothing());
  ASSERT_FALSE(storage_->IsValid(token));

  testing::Mock::VerifyAndClearExpectations(&mock_udac_);

  // And now we expect it to be called.
  EXPECT_CALL(mock_udac_, InvalidateAuthSession(_, _));

  // Return context, trigger invalidation.
  storage_->Return(token, std::move(context));
  ASSERT_FALSE(storage_->IsValid(token));
}

TEST_F(AuthSessionStorageImplTest, AsyncBorrow) {
  std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
  context->SetAuthSessionIds("some-id", "broadcast");

  // Unknown token, callback should
  // be called with nullptr immediately.
  {
    base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future;
    storage_->BorrowAsync(FROM_HERE, "unknown-token",
                          borrow_future.GetCallback());
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(borrow_future.IsReady());
    ASSERT_EQ(borrow_future.Get().get(), nullptr);
  }

  // Store UserContext;
  AuthProofToken token = storage_->Store(std::move(context));
  ASSERT_TRUE(storage_->IsValid(token));

  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future_1;
  storage_->BorrowAsync(FROM_HERE, token, borrow_future_1.GetCallback());
  // Borrow context, token is still valid.
  ASSERT_TRUE(storage_->IsValid(token));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(borrow_future_1.IsReady());
  ASSERT_NE(borrow_future_1.Get().get(), nullptr);

  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future_2;
  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future_3;

  storage_->BorrowAsync(FROM_HERE, token, borrow_future_2.GetCallback());
  storage_->BorrowAsync(FROM_HERE, token, borrow_future_3.GetCallback());

  ASSERT_TRUE(storage_->IsValid(token));

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(borrow_future_2.IsReady());
  ASSERT_FALSE(borrow_future_3.IsReady());

  // Return context, first in queue should get it
  storage_->Return(token, borrow_future_1.Take());
  ASSERT_TRUE(storage_->IsValid(token));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(borrow_future_2.IsReady());
  ASSERT_NE(borrow_future_2.Get().get(), nullptr);

  ASSERT_FALSE(borrow_future_3.IsReady());

  // Pending borrow request should be invalidated.
  storage_->Invalidate(token, base::DoNothing());
  ASSERT_FALSE(storage_->IsValid(token));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(borrow_future_3.IsReady());
  ASSERT_EQ(borrow_future_3.Get().get(), nullptr);

  // Token is considered invalid, callback should
  // be called with nullptr immediately.
  {
    base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future;
    storage_->BorrowAsync(FROM_HERE, token, borrow_future.GetCallback());

    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(borrow_future.IsReady());
    ASSERT_EQ(borrow_future.Get().get(), nullptr);
  }

  storage_->Return(token, borrow_future_2.Take());
  ASSERT_FALSE(storage_->IsValid(token));

  // Context should be deleted by now, callback should
  // be called with nullptr immediately.
  {
    base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future;
    storage_->BorrowAsync(FROM_HERE, token, borrow_future.GetCallback());

    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(borrow_future.IsReady());
    ASSERT_EQ(borrow_future.Get().get(), nullptr);
  }
}

}  // namespace ash