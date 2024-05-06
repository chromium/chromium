// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/cryptohome_core_impl.h"

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
#include "cryptohome_core_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CryptohomeCoreImplTest : public ::testing::Test {
 protected:
  CryptohomeCoreImplTest() {
    core_ = std::make_unique<CryptohomeCoreImpl>(&mock_udac_);
  }

  std::unique_ptr<UserContext> CreateUserContext() {
    std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
    context->SetAuthSessionIds("some-id", "broadcast");
    context->SetSessionLifetime(base::Time::Now() + base::Seconds(60));
    return context;
  }

  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::MockUserDataAuthClient mock_udac_;
  std::unique_ptr<CryptohomeCoreImpl> core_;
};

TEST_F(CryptohomeCoreImplTest, GetCurrentContext) {
  core_->ReturnContext(CreateUserContext());
  ASSERT_NE(core_->GetCurrentContext(), nullptr);
}

TEST_F(CryptohomeCoreImplTest, BorrowAndReturn) {
  core_->ReturnContext(CreateUserContext());

  // Simple paired borrow and return.
  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future;
  core_->BorrowContext(borrow_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(borrow_future.IsReady());
  ASSERT_NE(borrow_future.Get().get(), nullptr);
  core_->ReturnContext(borrow_future.Take());

  // Three borrows in a row.
  // The first borrow will get the context.
  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future_1;
  core_->BorrowContext(borrow_future_1.GetCallback());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(borrow_future_1.IsReady());
  ASSERT_NE(borrow_future_1.Get().get(), nullptr);

  // Two more borrows.
  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future_2;
  base::test::TestFuture<std::unique_ptr<UserContext>> borrow_future_3;
  core_->BorrowContext(borrow_future_2.GetCallback());
  core_->BorrowContext(borrow_future_3.GetCallback());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(borrow_future_2.IsReady());
  ASSERT_FALSE(borrow_future_3.IsReady());

  // Return context, first in queue should get it
  core_->ReturnContext(borrow_future_1.Take());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(borrow_future_2.IsReady());
  ASSERT_NE(borrow_future_2.Get().get(), nullptr);
  ASSERT_FALSE(borrow_future_3.IsReady());

  // Return another context, next in queue should get it
  core_->ReturnContext(borrow_future_2.Take());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(borrow_future_3.IsReady());
  ASSERT_NE(borrow_future_3.Get().get(), nullptr);

  // Return and check the context is restored.
  core_->ReturnContext(borrow_future_3.Take());
  ASSERT_NE(core_->GetCurrentContext(), nullptr);
}

}  // namespace ash
