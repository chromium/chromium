// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/extended_authenticator_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
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

void ExtendedAuthenticatorImpl::AuthenticateToMount(
    const UserContext& context,
    ResultCallback success_callback) {
  TransformKeyIfNeeded(
      context, base::BindOnce(&ExtendedAuthenticatorImpl::DoAuthenticateToMount,
                              this, std::move(success_callback)));
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
  CryptohomeClient::Get()->StartFingerprintAuthSession(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      cryptohome::StartFingerprintAuthSessionRequest(),
      base::BindOnce(
          &ExtendedAuthenticatorImpl::OnStartFingerprintAuthSessionComplete,
          this, std::move(callback)));
}

void ExtendedAuthenticatorImpl::OnStartFingerprintAuthSessionComplete(
    base::OnceCallback<void(bool)> callback,
    base::Optional<cryptohome::BaseReply> reply) {
  std::move(callback).Run(reply && !reply->has_error());
}

void ExtendedAuthenticatorImpl::EndFingerprintAuthSession() {
  CryptohomeClient::Get()->EndFingerprintAuthSession(
      cryptohome::EndFingerprintAuthSessionRequest(),
      base::BindOnce([](base::Optional<cryptohome::BaseReply> reply) {
        // Only check for existence of the reply, because if there is a reply,
        // it's always a BaseReply without errors.
        if (!reply)
          LOG(ERROR) << "EndFingerprintAuthSession call had no reply.";
      }));
}

void ExtendedAuthenticatorImpl::AuthenticateWithFingerprint(
    const UserContext& context,
    base::OnceCallback<void(cryptohome::CryptohomeErrorCode)> callback) {
  cryptohome::KeyDefinition key_def;
  key_def.type = cryptohome::KeyDefinition::TYPE_FINGERPRINT;
  CryptohomeClient::Get()->CheckKeyEx(
      cryptohome::CreateAccountIdentifierFromAccountId(context.GetAccountId()),
      cryptohome::CreateAuthorizationRequestFromKeyDef(key_def),
      cryptohome::CheckKeyRequest(),
      base::BindOnce(&ExtendedAuthenticatorImpl::OnFingerprintScanComplete,
                     this, std::move(callback)));
}

void ExtendedAuthenticatorImpl::OnFingerprintScanComplete(
    base::OnceCallback<void(cryptohome::CryptohomeErrorCode)> callback,
    base::Optional<cryptohome::BaseReply> reply) {
  if (!reply) {
    std::move(callback).Run(cryptohome::CryptohomeErrorCode::
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

void ExtendedAuthenticatorImpl::DoAuthenticateToMount(
    ResultCallback success_callback,
    const UserContext& user_context) {
  RecordStartMarker("MountEx");
  const Key* const key = user_context.GetKey();
  CryptohomeClient::Get()->MountEx(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(key->GetLabel(), key->GetSecret()),
      cryptohome::MountRequest(),
      base::BindOnce(&ExtendedAuthenticatorImpl::OnMountComplete, this,
                     "MountEx", user_context, std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::DoAuthenticateToCheck(
    base::OnceClosure success_callback,
    const UserContext& user_context) {
  RecordStartMarker("CheckKeyEx");
  cryptohome::HomedirMethods::GetInstance()->CheckKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequestFromKeyDef(
          cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
              user_context)),
      cryptohome::CheckKeyRequest(),
      base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                     "CheckKeyEx", user_context, std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::DoAddKey(const cryptohome::KeyDefinition& key,
                                         bool clobber_if_exists,
                                         base::OnceClosure success_callback,
                                         const UserContext& user_context) {
  RecordStartMarker("AddKeyEx");

  cryptohome::AddKeyRequest request;
  cryptohome::KeyDefinitionToKey(key, request.mutable_key());
  request.set_clobber_if_exists(clobber_if_exists);
  const Key* const auth_key = user_context.GetKey();
  cryptohome::HomedirMethods::GetInstance()->AddKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(auth_key->GetLabel(),
                                             auth_key->GetSecret()),
      request,
      base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                     "AddKeyEx", user_context, std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::DoRemoveKey(const std::string& key_to_remove,
                                            base::OnceClosure success_callback,
                                            const UserContext& user_context) {
  RecordStartMarker("RemoveKeyEx");

  cryptohome::RemoveKeyRequest request;
  request.mutable_key()->mutable_data()->set_label(key_to_remove);
  const Key* const auth_key = user_context.GetKey();
  cryptohome::HomedirMethods::GetInstance()->RemoveKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(auth_key->GetLabel(),
                                             auth_key->GetSecret()),
      request,
      base::BindOnce(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                     "RemoveKeyEx", user_context, std::move(success_callback)));
}

void ExtendedAuthenticatorImpl::OnMountComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    ResultCallback success_callback,
    base::Optional<cryptohome::BaseReply> reply) {
  cryptohome::MountError return_code =
      cryptohome::MountExReplyToMountError(reply);
  RecordEndMarker(time_marker);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    const std::string& mount_hash =
        cryptohome::MountExReplyToMountHash(reply.value());
    if (success_callback)
      std::move(success_callback).Run(mount_hash);
    if (consumer_) {
      UserContext copy = user_context;
      copy.SetUserIDHash(mount_hash);
      consumer_->OnAuthSuccess(copy);
    }
    return;
  }
  LOG(ERROR) << "MountEx failed. Error: " << return_code;
  AuthState state = FAILED_MOUNT;
  if (return_code == cryptohome::MOUNT_ERROR_TPM_COMM_ERROR ||
      return_code == cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK ||
      return_code == cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    state = FAILED_TPM;
  }
  if (return_code == cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST)
    state = NO_MOUNT;

  if (consumer_) {
    AuthFailure failure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
    consumer_->OnAuthFailure(failure);
  }
}

void ExtendedAuthenticatorImpl::OnOperationComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    base::OnceClosure success_callback,
    bool success,
    cryptohome::MountError return_code) {
  RecordEndMarker(time_marker);
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
