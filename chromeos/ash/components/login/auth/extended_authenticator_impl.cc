// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/extended_authenticator_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "components/account_id/account_id.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

// static
scoped_refptr<ExtendedAuthenticatorImpl> ExtendedAuthenticatorImpl::Create(
    AuthStatusConsumer* consumer) {
  auto extended_authenticator =
      base::WrapRefCounted(new ExtendedAuthenticatorImpl(consumer));
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &ExtendedAuthenticatorImpl::OnSaltObtained, extended_authenticator));
  return extended_authenticator;
}

ExtendedAuthenticatorImpl::ExtendedAuthenticatorImpl(
    AuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(consumer) {}

void ExtendedAuthenticatorImpl::SetConsumer(AuthStatusConsumer* consumer) {
  consumer_ = consumer;
}

void ExtendedAuthenticatorImpl::AuthenticateToCheck(
    const UserContext& context,
    base::OnceClosure success_callback) {
  TransformKeyIfNeeded(
      context, base::BindOnce(&ExtendedAuthenticatorImpl::DoAuthenticateToCheck,
                              this, std::move(success_callback),
                              /*unlock_webauthn_secret=*/false));
}

void ExtendedAuthenticatorImpl::AuthenticateToUnlockWebAuthnSecret(
    const UserContext& context,
    base::OnceClosure success_callback) {
  TransformKeyIfNeeded(
      context, base::BindOnce(&ExtendedAuthenticatorImpl::DoAuthenticateToCheck,
                              this, std::move(success_callback),
                              /*unlock_webauthn_secret=*/true));
}

void ExtendedAuthenticatorImpl::StartFingerprintAuthSession(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  user_data_auth::StartFingerprintAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  UserDataAuthClient::Get()->StartFingerprintAuthSession(
      request,
      base::BindOnce(
          &ExtendedAuthenticatorImpl::OnStartFingerprintAuthSessionComplete,
          this, std::move(callback)));
}

void ExtendedAuthenticatorImpl::OnStartFingerprintAuthSessionComplete(
    base::OnceCallback<void(bool)> callback,
    absl::optional<user_data_auth::StartFingerprintAuthSessionReply> reply) {
  std::move(callback).Run(
      reply.has_value() &&
      reply->error() ==
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
}

void ExtendedAuthenticatorImpl::EndFingerprintAuthSession() {
  UserDataAuthClient::Get()->EndFingerprintAuthSession(
      user_data_auth::EndFingerprintAuthSessionRequest(),
      base::BindOnce([](absl::optional<
                         user_data_auth::EndFingerprintAuthSessionReply>
                            reply) {
        if (!reply ||
            reply->error() !=
                user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
          LOG(ERROR) << "EndFingerprintAuthSession call failed with error: "
                     << (reply.has_value() ? static_cast<int>(reply->error())
                                           : -1);
        }
      }));
}

void ExtendedAuthenticatorImpl::AuthenticateWithFingerprint(
    const UserContext& context,
    base::OnceCallback<void(user_data_auth::CryptohomeErrorCode)> callback) {
  cryptohome::KeyDefinition key_def;
  key_def.type = cryptohome::KeyDefinition::TYPE_FINGERPRINT;
  user_data_auth::CheckKeyRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context.GetAccountId());
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequestFromKeyDef(key_def);
  UserDataAuthClient::Get()->CheckKey(
      request,
      base::BindOnce(&ExtendedAuthenticatorImpl::OnFingerprintScanComplete,
                     this, std::move(callback)));
}

void ExtendedAuthenticatorImpl::OnFingerprintScanComplete(
    base::OnceCallback<void(user_data_auth::CryptohomeErrorCode)> callback,
    absl::optional<user_data_auth::CheckKeyReply> reply) {
  if (!reply) {
    std::move(callback).Run(user_data_auth::CryptohomeErrorCode::
                                CRYPTOHOME_ERROR_FINGERPRINT_ERROR_INTERNAL);
    return;
  }

  std::move(callback).Run(reply->error());
}

void ExtendedAuthenticatorImpl::TransformKeyIfNeeded(
    const UserContext& user_context,
    ContextCallback callback) {
  if (user_context.GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN) {
    std::move(callback).Run(user_context);
    return;
  }

  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(
        base::BindOnce(&ExtendedAuthenticatorImpl::TransformKeyIfNeeded, this,
                       user_context, std::move(callback)));
    return;
  }

  UserContext transformed_context = user_context;
  transformed_context.GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                          system_salt_);
  std::move(callback).Run(transformed_context);
}

ExtendedAuthenticatorImpl::~ExtendedAuthenticatorImpl() = default;

void ExtendedAuthenticatorImpl::OnSaltObtained(const std::string& system_salt) {
  salt_obtained_ = true;
  system_salt_ = system_salt;
  for (auto& callback : system_salt_callbacks_)
    std::move(callback).Run();
  system_salt_callbacks_.clear();
}

void ExtendedAuthenticatorImpl::DoAuthenticateToCheck(
    base::OnceClosure success_callback,
    bool unlock_webauthn_secret,
    const UserContext& user_context) {
  LoginEventRecorder::Get()->AddLoginTimeMarker("Cryptohome-CheckKeyEx-Start",
                                                false);
  ::user_data_auth::CheckKeyRequest request;
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(user_context.GetAccountId()));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequestFromKeyDef(
          cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
              user_context));
  request.set_unlock_webauthn_secret(unlock_webauthn_secret);
  UserDataAuthClient::Get()->CheckKey(
      request, base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete<
                                  ::user_data_auth::CheckKeyReply>,
                              this, "Cryptohome-CheckKeyEx-End", user_context,
                              std::move(success_callback)));
}

template <typename ReplyType>
void ExtendedAuthenticatorImpl::OnOperationComplete(
    const char* time_marker,
    const UserContext& user_context,
    base::OnceClosure success_callback,
    absl::optional<ReplyType> reply) {
  LoginEventRecorder::Get()->AddLoginTimeMarker(time_marker, false);
  cryptohome::MountError return_code = cryptohome::MOUNT_ERROR_FATAL;
  if (reply.has_value()) {
    return_code = user_data_auth::CryptohomeErrorToMountError(reply->error());
  }

  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    if (success_callback)
      std::move(success_callback).Run();
    if (consumer_)
      consumer_->OnAuthSuccess(user_context);
    return;
  }

  LOG(ERROR) << "Extended authenticator cryptohome error, code: "
             << return_code;

  if (consumer_) {
    AuthFailure failure(AuthFailure::UNLOCK_FAILED);
    consumer_->OnAuthFailure(failure);
  }
}

}  // namespace ash
