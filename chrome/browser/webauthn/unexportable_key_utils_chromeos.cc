// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/unexportable_key_utils.h"

#include <cstdint>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"
#include "ui/aura/window.h"

namespace {

std::unique_ptr<crypto::UserVerifyingKeyProvider> (*g_test_override)() =
    nullptr;

// UserVerifyingSigningKeyCros wraps an `UnexportableSigningKey` and presents an
// Ash InSessionAuth dialog prior to releasing a signature. The wrapped key is
// currently not hardware bound.
class UserVerifyingSigningKeyCros : public crypto::UserVerifyingSigningKey {
 public:
  explicit UserVerifyingSigningKeyCros(
      UserVerifyingKeyProviderConfigChromeos config,
      std::unique_ptr<crypto::UnexportableSigningKey> signing_key,
      std::string label)
      : config_(std::move(config)),
        signing_key_(std::move(signing_key)),
        label_(std::move(label)) {
    CHECK(signing_key_);
  }

  ~UserVerifyingSigningKeyCros() override = default;

  void Sign(base::span<const uint8_t> data,
            UserVerifyingKeySignatureCallback callback) override {
    CHECK(config_.window);
    VerifyUser(base::BindOnce(
        &UserVerifyingSigningKeyCros::DoSign, weak_factory_.GetWeakPtr(),
        std::vector<uint8_t>(data.begin(), data.end()), std::move(callback)));
  }

  std::vector<uint8_t> GetPublicKey() const override {
    return signing_key_->GetSubjectPublicKeyInfo();
  }

  const crypto::UserVerifyingKeyLabel& GetKeyLabel() const override {
    return label_;
  }

 private:
  void VerifyUser(base::OnceCallback<void(bool)> callback) {
    auto visitor = base::Overloaded(
        // Legacy code path.
        [&](raw_ptr<ash::WebAuthNDialogController> controller) {
          CHECK(controller);
          controller->ShowAuthenticationDialog(config_.window, config_.rp_id,
                                               std::move(callback));
        },

        // New code path.
        [&](raw_ptr<ash::ActiveSessionAuthController> controller) {
          CHECK(controller);
          auto webauthn_auth_request =
              std::make_unique<ash::WebAuthNAuthRequest>(config_.rp_id,
                                                         std::move(callback));

          controller->ShowAuthDialog(std::move(webauthn_auth_request));
        });

    std::visit(visitor, config_.dialog_controller);
  }

  void DoSign(std::vector<uint8_t> data,
              UserVerifyingKeySignatureCallback callback,
              bool uv_success) {
    if (!uv_success) {
      std::move(callback).Run(base::unexpected(
          crypto::UserVerifyingKeySigningError::kUserCancellation));
      return;
    }
    std::optional<std::vector<uint8_t>> signature =
        signing_key_->SignSlowly(data);
    if (!signature) {
      std::move(callback).Run(base::unexpected(
          crypto::UserVerifyingKeySigningError::kUnknownError));
      return;
    }
    std::move(callback).Run(std::move(*signature));
  }

  const UserVerifyingKeyProviderConfigChromeos config_;
  const std::unique_ptr<crypto::UnexportableSigningKey> signing_key_;
  const std::string label_;

  base::WeakPtrFactory<UserVerifyingSigningKeyCros> weak_factory_{this};
};

class UserVerifyingKeyProviderCros : public crypto::UserVerifyingKeyProvider {
 public:
  explicit UserVerifyingKeyProviderCros(
      UserVerifyingKeyProviderConfigChromeos config)
      : config_(config) {}

  ~UserVerifyingKeyProviderCros() override = default;

  void GenerateUserVerifyingSigningKey(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      UserVerifyingKeyCreationCallback callback) override {
    std::unique_ptr<crypto::UnexportableSigningKey> key =
        crypto::GetSoftwareUnsecureUnexportableKeyProvider()
            ->GenerateSigningKeySlowly(acceptable_algorithms);
    std::string label = base::Base64Encode(key->GetWrappedKey());
    std::move(callback).Run(std::make_unique<UserVerifyingSigningKeyCros>(
        config_, std::move(key), std::move(label)));
  }

  void GetUserVerifyingSigningKey(
      crypto::UserVerifyingKeyLabel key_label,
      UserVerifyingKeyCreationCallback callback) override {
    std::optional<std::vector<uint8_t>> wrapped_private_key =
        base::Base64Decode(key_label);
    if (!wrapped_private_key) {
      std::move(callback).Run(
          base::unexpected(crypto::UserVerifyingKeyCreationError::kNotFound));
      return;
    }
    std::unique_ptr<crypto::UnexportableSigningKey> signing_key =
        crypto::GetSoftwareUnsecureUnexportableKeyProvider()
            ->FromWrappedSigningKeySlowly(*wrapped_private_key);
    std::move(callback).Run(std::make_unique<UserVerifyingSigningKeyCros>(
        config_, std::move(signing_key), std::move(key_label)));
  }

  void DeleteUserVerifyingKey(
      crypto::UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) override {
    // UserVerifyingSigningKeyCros is stateless, so this is a no-op.
    std::move(callback).Run(true);
  }

 private:
  UserVerifyingKeyProviderConfigChromeos config_;
};

}  // namespace

UserVerifyingKeyProviderConfigChromeos::UserVerifyingKeyProviderConfigChromeos(
    AuthDialogController dialog_controller,
    aura::Window* window,
    std::string rp_id)
    : dialog_controller(dialog_controller),
      window(window),
      rp_id(std::move(rp_id)) {}

UserVerifyingKeyProviderConfigChromeos::
    ~UserVerifyingKeyProviderConfigChromeos() = default;

UserVerifyingKeyProviderConfigChromeos::UserVerifyingKeyProviderConfigChromeos(
    const UserVerifyingKeyProviderConfigChromeos&) = default;

UserVerifyingKeyProviderConfigChromeos&
UserVerifyingKeyProviderConfigChromeos::operator=(
    const UserVerifyingKeyProviderConfigChromeos&) = default;

std::unique_ptr<crypto::UserVerifyingKeyProvider>
GetWebAuthnUserVerifyingKeyProvider(
    UserVerifyingKeyProviderConfigChromeos config) {
  if (g_test_override) {
    return g_test_override();
  }
  return std::make_unique<UserVerifyingKeyProviderCros>(std::move(config));
}

// ChromeOS doesn't use the UserVerifyingKeyProvider provided by //crypto, so
// the test override is handled separately as well.
void OverrideWebAuthnChromeosUserVerifyingKeyProviderForTesting(
    std::unique_ptr<crypto::UserVerifyingKeyProvider> (*func)()) {
  if (g_test_override) {
    // Prevent nesting of scoped providers.
    CHECK(!func);
    g_test_override = nullptr;
  } else {
    g_test_override = func;
  }
}
