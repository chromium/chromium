// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/unexportable_key_utils.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/auth/active_session_fingerprint_client.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"
#include "crypto/signature_verifier.h"
#include "crypto/user_verifying_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace {

constexpr char kRpId[] = "example.com";

using testing::_;

class MockWebAuthNDialogController : public ash::WebAuthNDialogController {
 public:
  MockWebAuthNDialogController() = default;

  ~MockWebAuthNDialogController() override = default;

  MOCK_METHOD(void, SetClient, (ash::InSessionAuthDialogClient*), (override));
  MOCK_METHOD(void,
              ShowAuthenticationDialog,
              (aura::Window * source_window,
               const std::string& origin_name,
               FinishCallback finish_callback),
              (override));
  MOCK_METHOD(void, DestroyAuthenticationDialog, (), (override));
  MOCK_METHOD(void,
              AuthenticateUserWithPasswordOrPin,
              (const std::string& password,
               bool authenticated_by_pin,
               OnAuthenticateCallback callback),
              (override));
  MOCK_METHOD(void,
              AuthenticateUserWithFingerprint,
              (base::OnceCallback<void(bool, ash::FingerprintState)>),
              (override));
  MOCK_METHOD(void, OpenInSessionAuthHelpPage, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(void,
              CheckAvailability,
              (FinishCallback on_availability_checked),
              (const, override));
};

class MockActiveSessionAuthController
    : public ash::ActiveSessionAuthController {
 public:
  MockActiveSessionAuthController() = default;

  ~MockActiveSessionAuthController() override = default;

  MOCK_METHOD(bool,
              ShowAuthDialog,
              (std::unique_ptr<ash::AuthRequest> auth_request),
              (override));
  MOCK_METHOD(bool, IsShown, (), (const override));
  MOCK_METHOD(void,
              SetFingerprintClient,
              (ash::ActiveSessionFingerprintClient * fp_client),
              (override));
};

class UserVerifyingKeyUtilsCrosTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  UserVerifyingKeyUtilsCrosTest() = default;
  ~UserVerifyingKeyUtilsCrosTest() override = default;
  UserVerifyingKeyUtilsCrosTest(const UserVerifyingKeyUtilsCrosTest&) = delete;
  UserVerifyingKeyUtilsCrosTest& operator=(
      const UserVerifyingKeyUtilsCrosTest&) = delete;

  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          ash::features::kWebAuthNAuthDialogMerge);
      return;
    }

    feature_list_.InitAndDisableFeature(
        ash::features::kWebAuthNAuthDialogMerge);
  }

  UserVerifyingKeyProviderConfigChromeos MakeKeyProviderConfig(
      aura::Window* window) {
    if (GetParam()) {
      return UserVerifyingKeyProviderConfigChromeos{&dialog_controller_, window,
                                                    kRpId};
    }

    return UserVerifyingKeyProviderConfigChromeos{&legacy_dialog_controller_,
                                                  window, kRpId};
  }

  void AssertRequestContainsRpId(ash::AuthRequest* request) {
    ASSERT_EQ(static_cast<ash::WebAuthNAuthRequest*>(request)->GetRpId(),
              kRpId);
  }

 protected:
  MockWebAuthNDialogController legacy_dialog_controller_;
  MockActiveSessionAuthController dialog_controller_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(UnexportableKeyUtils,
                         UserVerifyingKeyUtilsCrosTest,
                         testing::Bool());

TEST_P(UserVerifyingKeyUtilsCrosTest,
       UserVerifyingKeyProvider_GeneratedKeyCanBeImported) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(MakeKeyProviderConfig(window.get()));
  ASSERT_TRUE(provider);
  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      future;
  provider->GenerateUserVerifyingSigningKey(
      {{crypto::SignatureVerifier::ECDSA_SHA256}}, future.GetCallback());
  crypto::UserVerifyingSigningKey& signing_key = *future.Get().value();

  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      get_future;
  provider->GetUserVerifyingSigningKey(signing_key.GetKeyLabel(),
                                       get_future.GetCallback());
  crypto::UserVerifyingSigningKey& imported_signing_key =
      *get_future.Get().value();
  EXPECT_EQ(signing_key.GetPublicKey(), imported_signing_key.GetPublicKey());
}

TEST_P(UserVerifyingKeyUtilsCrosTest,
       UserVerifyingKeyProvider_SigningShowsInSessionAuthChallenge) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(MakeKeyProviderConfig(window.get()));
  ASSERT_TRUE(provider);

  // Simulate successful in-session auth.
  if (GetParam()) {
    EXPECT_CALL(dialog_controller_, ShowAuthDialog(_))
        .WillOnce([this](std::unique_ptr<ash::AuthRequest> request) {
          AssertRequestContainsRpId(request.get());
          request->NotifyAuthSuccess(nullptr);
          return true;
        });
  } else {
    EXPECT_CALL(legacy_dialog_controller_,
                ShowAuthenticationDialog(window.get(), kRpId, _))
        .WillOnce([](aura::Window*, const std::string& rp_id,
                     base::OnceCallback<void(bool)> callback) {
          ASSERT_EQ(rp_id, kRpId);
          std::move(callback).Run(true);
        });
  }

  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      future;
  provider->GenerateUserVerifyingSigningKey(
      {{crypto::SignatureVerifier::ECDSA_SHA256}}, future.GetCallback());
  crypto::UserVerifyingSigningKey& signing_key = *future.Get().value();
  base::test::TestFuture<base::expected<std::vector<uint8_t>,
                                        crypto::UserVerifyingKeySigningError>>
      signature_future;
  signing_key.Sign({{1, 2, 3}}, signature_future.GetCallback());
  EXPECT_TRUE(signature_future.Get().has_value());
  EXPECT_FALSE(signature_future.Get()->empty());
}

TEST_P(UserVerifyingKeyUtilsCrosTest,
       UserVerifyingKeyProvider_SigningWithoutUvYieldsNullopt) {
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(MakeKeyProviderConfig(window.get()));
  ASSERT_TRUE(provider);

  // Simulate failed in-session auth.
  if (GetParam()) {
    EXPECT_CALL(dialog_controller_, ShowAuthDialog(_))
        .WillOnce([this](std::unique_ptr<ash::AuthRequest> request) -> bool {
          AssertRequestContainsRpId(request.get());
          request->NotifyAuthFailure();
          return true;
        });
  } else {
    EXPECT_CALL(legacy_dialog_controller_,
                ShowAuthenticationDialog(window.get(), kRpId, _))
        .WillOnce([](aura::Window*, const std::string& rp_id,
                     base::OnceCallback<void(bool)> callback) {
          ASSERT_EQ(rp_id, kRpId);
          std::move(callback).Run(false);
        });
  }

  base::test::TestFuture<
      base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                     crypto::UserVerifyingKeyCreationError>>
      signing_key_future;
  provider->GenerateUserVerifyingSigningKey(
      {{crypto::SignatureVerifier::ECDSA_SHA256}},
      signing_key_future.GetCallback());
  base::test::TestFuture<base::expected<std::vector<uint8_t>,
                                        crypto::UserVerifyingKeySigningError>>
      signature_future;
  (*signing_key_future.Get())
      ->Sign({{1, 2, 3}}, signature_future.GetCallback());
  EXPECT_FALSE(signature_future.Get().has_value());
}

TEST_P(UserVerifyingKeyUtilsCrosTest, UserVerifyingKeyProvider_DeleteIsANoOp) {
  std::unique_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(MakeKeyProviderConfig(w1.get()));
  ASSERT_TRUE(provider);
  base::test::TestFuture<bool> future;
  provider->DeleteUserVerifyingKey("test key label", future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace
