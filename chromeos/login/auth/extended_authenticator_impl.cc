// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/extended_authenticator_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/cryptohome/userdataauth_util.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/login_event_recorder.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace {

void RecordStartMarker(const std::string& marker) {
  std::string full_marker = "Cryptohome-";
  full_marker.append(marker);
  full_marker.append("-Start");
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(full_marker, false);
}

void RecordEndMarker(const std::string& marker) {
  std::string full_marker = "Cryptohome-";
  full_marker.append(marker);
  full_marker.append("-End");
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(full_marker, false);
}

}  // namespace

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
                              this, std::move(success_callback)));
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

void ExtendedAuthenticatorImpl::AddKey(const UserContext& context,
                                       const cryptohome::KeyDefinition& key,
                                       bool clobber_if_exists,
                                       base::OnceClosure success_callback) {
  TransformKeyIfNeeded(
      context, base::BindOnce(&ExtendedAuthenticatorImpl::DoAddKey, this, key,
                              clobber_if_exists, std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::RemoveKey(const UserContext& context,
                                          const std::string& key_to_remove,
                                          base::OnceClosure success_callback) {
  TransformKeyIfNeeded(
      context, base::BindOnce(&ExtendedAuthenticatorImpl::DoRemoveKey, this,
                              key_to_remove, std::move(success_callback)));
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
    const UserContext& user_context) {
  RecordStartMarker("CheckKeyEx");
  ::user_data_auth::CheckKeyRequest request;
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(user_context.GetAccountId()));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequestFromKeyDef(
          cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
              user_context));
  chromeos::UserDataAuthClient::Get()->CheckKey(
      request, base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete<
                                  ::user_data_auth::CheckKeyReply>,
                              this, "CheckKeyEx", user_context,
                              std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::DoAddKey(const cryptohome::KeyDefinition& key,
                                         bool clobber_if_exists,
                                         base::OnceClosure success_callback,
                                         const UserContext& user_context) {
  RecordStartMarker("AddKeyEx");

  ::user_data_auth::AddKeyRequest request;
  cryptohome::KeyDefinitionToKey(key, request.mutable_key());
  request.set_clobber_if_exists(clobber_if_exists);
  const Key* const auth_key = user_context.GetKey();
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(user_context.GetAccountId()));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequest(auth_key->GetLabel(),
                                             auth_key->GetSecret());
  chromeos::UserDataAuthClient::Get()->AddKey(
      request, base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete<
                                  ::user_data_auth::AddKeyReply>,
                              this, "AddKeyEx", user_context,
                              std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::DoRemoveKey(const std::string& key_to_remove,
                                            base::OnceClosure success_callback,
                                            const UserContext& user_context) {
  RecordStartMarker("RemoveKeyEx");

  ::user_data_auth::RemoveKeyRequest request;
  request.mutable_key()->mutable_data()->set_label(key_to_remove);
  const Key* const auth_key = user_context.GetKey();
  *request.mutable_account_id() = CreateAccountIdentifierFromIdentification(
      cryptohome::Identification(user_context.GetAccountId()));
  *request.mutable_authorization_request() =
      cryptohome::CreateAuthorizationRequest(auth_key->GetLabel(),
                                             auth_key->GetSecret());
  chromeos::UserDataAuthClient::Get()->RemoveKey(
      request, base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete<
                                  ::user_data_auth::RemoveKeyReply>,
                              this, "RemoveKeyEx", user_context,
                              std::move(success_callback)));
}

template <typename ReplyType>
void ExtendedAuthenticatorImpl::OnOperationComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    base::OnceClosure success_callback,
    absl::optional<ReplyType> reply) {
  RecordEndMarker(time_marker);
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

  AuthState state = FAILED_MOUNT;

  if (return_code == cryptohome::MOUNT_ERROR_TPM_COMM_ERROR ||
      return_code == cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK ||
      return_code == cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    state = FAILED_TPM;
  }

  if (return_code == cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST)
    state = NO_MOUNT;

  if (consumer_) {
    AuthFailure failure(AuthFailure::UNLOCK_FAILED);
    consumer_->OnAuthFailure(failure);
  }
}

}  // namespace chromeos
