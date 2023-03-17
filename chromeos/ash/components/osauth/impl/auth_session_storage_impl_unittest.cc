// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_session_storage_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/location.h"
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
  ash::MockUserDataAuthClient mock_udac_;
  std::unique_ptr<AuthSessionStorageImpl> storage_;
};

TEST_F(AuthSessionStorageImplTest, Basic) {
  std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
  context->SetAuthSessionId("some-id");

  // Store UserContext;

  AuthProofToken token = storage_->Store(std::move(context));
  ASSERT_TRUE(storage_->IsValid(token));

  // Borrow context, token is still valid.
  context = storage_->Borrow(FROM_HERE, token);
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
  context->SetAuthSessionId("some-id");

  // Store UserContext;
  AuthProofToken token = storage_->Store(std::move(context));
  ASSERT_TRUE(storage_->IsValid(token));
  // Borrow context, token is still valid.
  context = storage_->Borrow(FROM_HERE, token);
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

}  // namespace ash