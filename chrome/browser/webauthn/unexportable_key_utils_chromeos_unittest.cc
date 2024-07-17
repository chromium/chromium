// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/unexportable_key_utils.h"

#include <memory>
#include <string>

#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/test/test_future.h"
#include "crypto/signature_verifier.h"
#include "crypto/user_verifying_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace {

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

TEST(UserVerifyingKeyUtilsCrosTest,
     UserVerifyingKeyProvider_GeneratedKeyCanBeImported) {
  MockWebAuthNDialogController dialog_controller;
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(
          UserVerifyingKeyProviderConfigChromeos{
              .dialog_controller = &dialog_controller, .window = window.get()});
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

TEST(UserVerifyingKeyUtilsCrosTest,
     UserVerifyingKeyProvider_SigningShowsInSessionAuthChallenge) {
  MockWebAuthNDialogController dialog_controller;
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(
          UserVerifyingKeyProviderConfigChromeos{
              .dialog_controller = &dialog_controller, .window = window.get()});
  ASSERT_TRUE(provider);

  // Simulate successful in-session auth.
  EXPECT_CALL(dialog_controller, ShowAuthenticationDialog(window.get(), "", _))
      .WillOnce([](aura::Window*, const std::string&,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

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

TEST(UserVerifyingKeyUtilsCrosTest,
     UserVerifyingKeyProvider_SigningWithoutUvYieldsNullopt) {
  MockWebAuthNDialogController dialog_controller;
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(
          UserVerifyingKeyProviderConfigChromeos{
              .dialog_controller = &dialog_controller, .window = window.get()});
  ASSERT_TRUE(provider);

  // Simulate failed in-session auth.
  EXPECT_CALL(dialog_controller, ShowAuthenticationDialog(window.get(), "", _))
      .WillOnce([](aura::Window*, const std::string&,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });

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

TEST(UserVerifyingKeyUtilsCrosTest, UserVerifyingKeyProvider_DeleteIsANoOp) {
  MockWebAuthNDialogController dialog_controller;
  std::unique_ptr<aura::Window> w1(
      aura::test::CreateTestWindowWithId(1, nullptr));
  std::unique_ptr<crypto::UserVerifyingKeyProvider> provider =
      GetWebAuthnUserVerifyingKeyProvider(
          UserVerifyingKeyProviderConfigChromeos{
              .dialog_controller = &dialog_controller, .window = w1.get()});
  ASSERT_TRUE(provider);
  base::test::TestFuture<bool> future;
  provider->DeleteUserVerifyingKey("test key label", future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace
