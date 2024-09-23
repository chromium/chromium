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
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::_;
using testing::Eq;
using testing::Invoke;

class AuthSessionStorageImplTest : public ::testing::Test {
 protected:
  AuthSessionStorageImplTest() {
    storage_ = std::make_unique<AuthSessionStorageImpl>(
        &mock_udac_, base::DefaultClock::GetInstance());
  }

  std::unique_ptr<UserContext> CreateContext() {
    std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
    context->SetAuthSessionIds("some-id", "broadcast");
    context->SetSessionLifetime(base::Time::Now() + base::Seconds(60));
    return context;
  }

  void ExpectExtendAuthsession(ash::MockUserDataAuthClient& mock_client) {
    EXPECT_CALL(mock_udac_, ExtendAuthSession(_, _))
        .WillOnce(
            Invoke([](const ::user_data_auth::ExtendAuthSessionRequest& request,
                      UserDataAuthClient::ExtendAuthSessionCallback callback) {
              ::user_data_auth::ExtendAuthSessionReply reply;
              reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
              reply.set_seconds_left(request.extension_duration());
              std::move(callback).Run(reply);
            }));
  }

  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::MockUserDataAuthClient mock_udac_;
  std::unique_ptr<AuthSessionStorageImpl> storage_;
};

TEST_F(AuthSessionStorageImplTest, Basic) {
  AuthProofToken token = storage_->Store(CreateContext());

  ASSERT_TRUE(storage_->IsValid(token));

  // Borrow context, token is still valid.
  auto context = storage_->BorrowForTests(FROM_HERE, token);
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
  AuthProofToken token = storage_->Store(CreateContext());

  ASSERT_TRUE(storage_->IsValid(token));
  // Borrow context, token is still valid.
  auto context = storage_->BorrowForTests(FROM_HERE, token);
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
  std::unique_ptr<UserContext> context = CreateContext();

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

TEST_F(AuthSessionStorageImplTest, LifetimeInvalidateUponTimer) {
  AuthProofToken token = storage_->Store(CreateContext());

  ASSERT_TRUE(storage_->IsValid(token));

  EXPECT_CALL(mock_udac_, InvalidateAuthSession(_, _));

  // Should trigger invalidating AuthSession.
  task_environment.AdvanceClock(base::Seconds(61));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(storage_->IsValid(token));
}

TEST_F(AuthSessionStorageImplTest, LifetimeExtendWhenIdle) {
  AuthProofToken token = storage_->Store(CreateContext());
  base::Time start = base::Time::Now();

  ASSERT_TRUE(storage_->IsValid(token));
  auto keep_alive = storage_->KeepAlive(token);

  ExpectExtendAuthsession(mock_udac_);

  // Should trigger extending AuthSession.
  task_environment.AdvanceClock(base::Seconds(59));
  base::RunLoop().RunUntilIdle();
  task_environment.AdvanceClock(base::Seconds(31));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(storage_->IsValid(token));
  ASSERT_GE(storage_->Peek(token)->GetSessionLifetime(),
            start + base::Seconds(90));
  keep_alive.reset();
}

TEST_F(AuthSessionStorageImplTest, LifetimeExtendUponReturn) {
  AuthProofToken token = storage_->Store(CreateContext());
  base::Time start = base::Time::Now();

  ASSERT_TRUE(storage_->IsValid(token));
  auto keep_alive = storage_->KeepAlive(token);

  auto borrowed = storage_->BorrowForTests(FROM_HERE, token);

  // Not extending session as context is borrowed.
  task_environment.AdvanceClock(base::Seconds(59));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_udac_);

  // Session would be extended as soon as context is returned.
  ExpectExtendAuthsession(mock_udac_);

  storage_->Return(token, std::move(borrowed));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(storage_->IsValid(token));
  ASSERT_GE(storage_->Peek(token)->GetSessionLifetime(),
            start + base::Seconds(60));
  keep_alive.reset();
}

TEST_F(AuthSessionStorageImplTest, LifetimeInvalidateUponReturningTooLate) {
  AuthProofToken token = storage_->Store(CreateContext());

  ASSERT_TRUE(storage_->IsValid(token));
  auto keep_alive = storage_->KeepAlive(token);

  auto borrowed = storage_->BorrowForTests(FROM_HERE, token);

  // Not invalidating session as context is borrowed.
  task_environment.AdvanceClock(base::Seconds(61));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_udac_);

  // Session would be extended as soon as context is returned.
  EXPECT_CALL(mock_udac_, InvalidateAuthSession(_, _))
      .WillOnce(Invoke(
          [](const ::user_data_auth::InvalidateAuthSessionRequest& request,
             UserDataAuthClient::InvalidateAuthSessionCallback callback) {
            ::user_data_auth::InvalidateAuthSessionReply reply;
            reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
            std::move(callback).Run(reply);
          }));

  storage_->Return(token, std::move(borrowed));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(storage_->IsValid(token));
  keep_alive.reset();
}

TEST_F(AuthSessionStorageImplTest, LifetimeInvalidateUponTimerHadKeepalive) {
  // Store UserContext;
  AuthProofToken token = storage_->Store(CreateContext());
  ASSERT_TRUE(storage_->IsValid(token));
  auto keep_alive = storage_->KeepAlive(token);

  task_environment.AdvanceClock(base::Seconds(30));
  base::RunLoop().RunUntilIdle();
  keep_alive.reset();

  EXPECT_CALL(mock_udac_, InvalidateAuthSession(_, _));

  // Should trigger invalidating AuthSession.
  task_environment.AdvanceClock(base::Seconds(31));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(storage_->IsValid(token));
}

TEST_F(AuthSessionStorageImplTest, WithdrawTest) {
  AuthProofToken token = storage_->Store(CreateContext());
  ASSERT_TRUE(storage_->IsValid(token));

  base::test::TestFuture<std::unique_ptr<UserContext>> withdraw_future;

  storage_->Withdraw(token, withdraw_future.GetCallback());

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(withdraw_future.IsReady());
  ASSERT_NE(withdraw_future.Get().get(), nullptr);

  ASSERT_FALSE(storage_->IsValid(token));
}

TEST_F(AuthSessionStorageImplTest, WithdrawPreceedsBorrow) {
  AuthProofToken token = storage_->Store(CreateContext());
  ASSERT_TRUE(storage_->IsValid(token));

  // Borrow context, token is still valid.
  auto context = storage_->BorrowForTests(FROM_HERE, token);
  ASSERT_TRUE(storage_->IsValid(token));

  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future;
  base::test::TestFuture<std::unique_ptr<UserContext>> withdraw_future;

  storage_->BorrowAsync(FROM_HERE, token, borrow_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(borrow_future.IsReady());

  storage_->Withdraw(token, withdraw_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(withdraw_future.IsReady());

  // Return context, token still valid
  storage_->Return(token, std::move(context));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(withdraw_future.IsReady());
  ASSERT_TRUE(borrow_future.IsReady());

  ASSERT_EQ(borrow_future.Get().get(), nullptr);
  ASSERT_NE(withdraw_future.Get().get(), nullptr);
  ASSERT_FALSE(storage_->IsValid(token));
}

}  // namespace ash