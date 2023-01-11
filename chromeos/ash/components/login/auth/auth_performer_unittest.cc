// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_performer.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/auth_session_status.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

using ::cryptohome::KeyLabel;
using ::testing::_;

void SetupUserWithLegacyPasswordFactor(UserContext* context) {
  std::vector<cryptohome::AuthFactor> factors;
  cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                                KeyLabel("legacy-0"));
  cryptohome::AuthFactor factor(ref, cryptohome::AuthFactorCommonMetadata());
  factors.push_back(factor);
  SessionAuthFactors data(factors);
  context->SetSessionAuthFactors(data);
}

void ReplyAsSuccess(
    UserDataAuthClient::AuthenticateAuthFactorCallback callback) {
  ::user_data_auth::AuthenticateAuthFactorReply reply;
  reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
  reply.set_authenticated(true);
  reply.add_authorized_for(user_data_auth::AUTH_INTENT_DECRYPT);
  std::move(callback).Run(reply);
}

void ReplyAsKeyMismatch(
    UserDataAuthClient::AuthenticateAuthFactorCallback callback) {
  ::user_data_auth::AuthenticateAuthFactorReply reply;
  reply.set_error(
      ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
  reply.set_authenticated(false);
  std::move(callback).Run(reply);
}

class AuthPerformerTest : public testing::Test {
 public:
  AuthPerformerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {
    CryptohomeMiscClient::InitializeFake();
    SystemSaltGetter::Initialize();
    context_ = std::make_unique<UserContext>();
  }

  ~AuthPerformerTest() override {
    SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ::testing::StrictMock<MockUserDataAuthClient> mock_client_;
  std::unique_ptr<UserContext> context_;
};

// Checks that a key that has no type is recognized during StartAuthSession() as
// a password knowledge key.
TEST_F(AuthPerformerTest, StartWithUntypedPasswordKey) {
  // Arrange: cryptohome replies with a key that has no |type| set.
  EXPECT_CALL(mock_client_, StartAuthSession(_, _))
      .WillOnce([](const ::user_data_auth::StartAuthSessionRequest& request,
                   UserDataAuthClient::StartAuthSessionCallback callback) {
        ::user_data_auth::StartAuthSessionReply reply;
        reply.set_auth_session_id("123");
        reply.set_user_exists(true);
        auto* factor = reply.add_auth_factors();
        factor->set_label("legacy-0");
        factor->set_type(
            user_data_auth::AuthFactorType::AUTH_FACTOR_TYPE_UNSPECIFIED);
        std::move(callback).Run(reply);
      });
  AuthPerformer performer(&mock_client_);

  // Act.
  base::test::TestFuture<bool, std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;
  performer.StartAuthSession(std::move(context_), /*ephemeral=*/false,
                             AuthSessionIntent::kDecrypt, result.GetCallback());
  auto [user_exists, user_context, cryptohome_error] = result.Take();

  // Assert: no error, user context has AuthSession ID and the password factor.
  EXPECT_TRUE(user_exists);
  ASSERT_TRUE(user_context);
  EXPECT_EQ(user_context->GetAuthSessionId(), "123");
  EXPECT_TRUE(user_context->GetAuthFactorsData().FindOnlinePasswordFactor());
}

// Checks that a key that has no type is recognized during StartAuthSession() as
// a kiosk key for a kiosk user.
TEST_F(AuthPerformerTest, StartWithUntypedKioskKey) {
  // Arrange: user is kiosk, and cryptohome replies with a key that has no
  // |type| set.
  context_ = std::make_unique<UserContext>(user_manager::USER_TYPE_KIOSK_APP,
                                           AccountId());
  EXPECT_CALL(mock_client_, StartAuthSession(_, _))
      .WillOnce([](const ::user_data_auth::StartAuthSessionRequest& request,
                   UserDataAuthClient::StartAuthSessionCallback callback) {
        ::user_data_auth::StartAuthSessionReply reply;
        reply.set_auth_session_id("123");
        reply.set_user_exists(true);
        auto* factor = reply.add_auth_factors();
        factor->set_label("legacy-0");
        factor->set_type(
            user_data_auth::AuthFactorType::AUTH_FACTOR_TYPE_UNSPECIFIED);
        std::move(callback).Run(reply);
      });
  AuthPerformer performer(&mock_client_);

  // Act.
  base::test::TestFuture<bool, std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;
  performer.StartAuthSession(std::move(context_), /*ephemeral=*/false,
                             AuthSessionIntent::kDecrypt, result.GetCallback());
  auto [user_exists, user_context, cryptohome_error] = result.Take();

  // Assert: no error, user context has AuthSession ID and the kiosk factor.
  EXPECT_TRUE(user_exists);
  ASSERT_TRUE(user_context);
  EXPECT_EQ(user_context->GetAuthSessionId(), "123");
  EXPECT_TRUE(user_context->GetAuthFactorsData().FindKioskFactor());
}

// Checks that AuthenticateUsingKnowledgeKey (which will be called with "gaia"
// label after online authentication) correctly falls back to "legacy-0" label.
TEST_F(AuthPerformerTest, KnowledgeKeyCorrectLabelFallback) {
  SetupUserWithLegacyPasswordFactor(context_.get());
  // Password knowledge key in user context.
  *context_->GetKey() = Key("secret");
  context_->GetKey()->SetLabel("gaia");
  // Simulate the already started auth session.
  context_->SetAuthSessionId("123");

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthFactor(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthFactorRequest& request,
             UserDataAuthClient::AuthenticateAuthFactorCallback callback) {
            EXPECT_EQ(request.auth_factor_label(), "legacy-0");
            EXPECT_TRUE(request.has_auth_input());
            EXPECT_TRUE(request.auth_input().has_password_input());
            ReplyAsSuccess(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;
  performer.AuthenticateUsingKnowledgeKey(std::move(context_),
                                          result.GetCallback());
  // Check for no error, and user context is present
  ASSERT_FALSE(result.Get<1>().has_value());
  ASSERT_TRUE(result.Get<0>());
}

// Checks that AuthenticateUsingKnowledgeKey called with "pin" key does not
// fallback to "legacy-0" label.
TEST_F(AuthPerformerTest, KnowledgeKeyNoFallbackOnPin) {
  SetupUserWithLegacyPasswordFactor(context_.get());
  // Simulate the already started auth session.
  context_->SetAuthSessionId("123");

  // PIN knowledge key in user context.
  *context_->GetKey() =
      Key(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, "salt", /*secret=*/"123456");
  context_->GetKey()->SetLabel("pin");
  context_->SetIsUsingPin(true);

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthFactor(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthFactorRequest& request,
             UserDataAuthClient::AuthenticateAuthFactorCallback callback) {
            EXPECT_EQ(request.auth_factor_label(), "pin");
            EXPECT_TRUE(request.has_auth_input());
            EXPECT_TRUE(request.auth_input().has_pin_input());
            ReplyAsKeyMismatch(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;
  performer.AuthenticateUsingKnowledgeKey(std::move(context_),
                                          result.GetCallback());
  // Check that the error is present, and user context is passed back.
  ASSERT_TRUE(result.Get<0>());
  ASSERT_TRUE(result.Get<1>().has_value());
  ASSERT_EQ(result.Get<1>().value().get_cryptohome_code(),
            user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
}

TEST_F(AuthPerformerTest, AuthenticateWithPasswordCorrectLabel) {
  SetupUserWithLegacyPasswordFactor(context_.get());
  // Simulate the already started auth session.
  context_->SetAuthSessionId("123");

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthFactor(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthFactorRequest& request,
             UserDataAuthClient::AuthenticateAuthFactorCallback callback) {
            EXPECT_EQ(request.auth_factor_label(), "legacy-0");
            EXPECT_TRUE(request.has_auth_input());
            EXPECT_TRUE(request.auth_input().has_password_input());
            EXPECT_FALSE(
                request.auth_input().password_input().secret().empty());
            ReplyAsSuccess(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;

  performer.AuthenticateWithPassword("legacy-0", "secret", std::move(context_),
                                     result.GetCallback());
  // Check for no error
  ASSERT_TRUE(result.Get<0>());
  ASSERT_FALSE(result.Get<1>().has_value());
}

TEST_F(AuthPerformerTest, AuthenticateWithPasswordBadLabel) {
  SetupUserWithLegacyPasswordFactor(context_.get());
  // Simulate the already started auth session.
  context_->SetAuthSessionId("123");

  AuthPerformer performer(&mock_client_);

  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;

  performer.AuthenticateWithPassword("gaia", "secret", std::move(context_),
                                     result.GetCallback());

  // Check that error is triggered
  ASSERT_TRUE(result.Get<0>());
  ASSERT_TRUE(result.Get<1>().has_value());
  ASSERT_EQ(result.Get<1>().value().get_cryptohome_code(),
            user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
}

TEST_F(AuthPerformerTest, AuthenticateWithPinSuccess) {
  SetupUserWithLegacyPasswordFactor(context_.get());
  // Simulate the already started auth session.
  context_->SetAuthSessionId("123");

  // Add a pin factor to session auth factors.
  cryptohome::AuthFactorRef pin_factor_ref(cryptohome::AuthFactorType::kPin,
                                           cryptohome::KeyLabel("pin"));
  cryptohome::AuthFactor pin_factor(
      std::move(pin_factor_ref), cryptohome::AuthFactorCommonMetadata(),
      cryptohome::PinStatus{.auth_locked = false});
  context_->SetSessionAuthFactors(SessionAuthFactors({std::move(pin_factor)}));

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthFactor(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthFactorRequest& request,
             UserDataAuthClient::AuthenticateAuthFactorCallback callback) {
            EXPECT_EQ(request.auth_factor_label(), "pin");
            EXPECT_TRUE(request.has_auth_input());
            EXPECT_TRUE(request.auth_input().has_pin_input());
            EXPECT_FALSE(request.auth_input().pin_input().secret().empty());
            ReplyAsSuccess(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<AuthenticationError>>
      result;

  performer.AuthenticateWithPin("1234", "pin-salt", std::move(context_),
                                result.GetCallback());
  // Check for no error
  ASSERT_TRUE(result.Get<0>());
  ASSERT_FALSE(result.Get<1>().has_value());
}

}  // namespace
}  // namespace ash
