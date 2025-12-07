// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"

#include <optional>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
using ::testing::_;
using ::user_data_auth::LockFactorUntilRebootReply;
using ::user_data_auth::LockFactorUntilRebootRequest;
using LockFactorUntilRebootCallback =
    UserDataAuthClient::LockFactorUntilRebootCallback;

void ReplyAsSuccess(LockFactorUntilRebootCallback callback) {
  LockFactorUntilRebootReply reply;
  reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
  std::move(callback).Run(reply);
}

void ReplyAsError(LockFactorUntilRebootCallback callback) {
  LockFactorUntilRebootReply reply;
  reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_IMPLEMENTED);
  std::move(callback).Run(reply);
}

}  // namespace

class AuthFactorEditorTest : public testing::Test {
 public:
  AuthFactorEditorTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {
    CryptohomeMiscClient::InitializeFake();
    SystemSaltGetter::Initialize();
  }

  ~AuthFactorEditorTest() override {
    SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ::testing::StrictMock<MockUserDataAuthClient> mock_client_;
};

TEST_F(AuthFactorEditorTest, LockCryptohomeRecoveryUntilRebootSuccess) {
  EXPECT_CALL(mock_client_, LockFactorUntilReboot(_, _))
      .WillOnce([](const LockFactorUntilRebootRequest& request,
                   LockFactorUntilRebootCallback callback) {
        ReplyAsSuccess(std::move(callback));
      });

  AuthFactorEditor auth_factor(&mock_client_);
  base::test::TestFuture<std::optional<AuthenticationError>> result;

  auth_factor.LockCryptohomeRecoveryUntilReboot(result.GetCallback());
  auto auth_error = result.Take();

  EXPECT_FALSE(auth_error.has_value());
}

TEST_F(AuthFactorEditorTest, LockCryptohomeRecoveryUntilRebootFailure) {
  EXPECT_CALL(mock_client_, LockFactorUntilReboot(_, _))
      .WillOnce([](const LockFactorUntilRebootRequest& request,
                   LockFactorUntilRebootCallback callback) {
        ReplyAsError(std::move(callback));
      });

  AuthFactorEditor auth_factor(&mock_client_);
  base::test::TestFuture<std::optional<AuthenticationError>> result;

  auth_factor.LockCryptohomeRecoveryUntilReboot(result.GetCallback());
  auto auth_error = result.Take();

  EXPECT_TRUE(auth_error.has_value());
  EXPECT_EQ(auth_error->get_origin(), AuthenticationError::Origin::kCryptohome);
  EXPECT_EQ(auth_error->get_cryptohome_code(),
            ::user_data_auth::CRYPTOHOME_ERROR_NOT_IMPLEMENTED);
}

}  // namespace ash
