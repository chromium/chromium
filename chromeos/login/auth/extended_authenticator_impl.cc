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
    NewAuthStatusConsumer* consumer) {
  auto extended_authenticator =
      base::WrapRefCounted(new ExtendedAuthenticatorImpl(consumer));
  SystemSaltGetter::Get()->GetSystemSalt(base::Bind(
      &ExtendedAuthenticatorImpl::OnSaltObtained, extended_authenticator));
  return extended_authenticator;
}

// static
scoped_refptr<ExtendedAuthenticatorImpl> ExtendedAuthenticatorImpl::Create(
    AuthStatusConsumer* consumer) {
  auto extended_authenticator =
      base::WrapRefCounted(new ExtendedAuthenticatorImpl(consumer));
  SystemSaltGetter::Get()->GetSystemSalt(base::Bind(
      &ExtendedAuthenticatorImpl::OnSaltObtained, extended_authenticator));
  return extended_authenticator;
}

ExtendedAuthenticatorImpl::ExtendedAuthenticatorImpl(
    NewAuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(consumer), old_consumer_(NULL) {
}

ExtendedAuthenticatorImpl::ExtendedAuthenticatorImpl(
    AuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(NULL), old_consumer_(consumer) {
}

void ExtendedAuthenticatorImpl::SetConsumer(AuthStatusConsumer* consumer) {
  old_consumer_ = consumer;
}

void ExtendedAuthenticatorImpl::AuthenticateToMount(
    const UserContext& context,
    const ResultCallback& success_callback) {
  TransformKeyIfNeeded(
      context,
      base::Bind(&ExtendedAuthenticatorImpl::DoAuthenticateToMount,
                 this,
                 success_callback));
}

void ExtendedAuthenticatorImpl::AuthenticateToCheck(
    const UserContext& context,
    const base::Closure& success_callback) {
  TransformKeyIfNeeded(
      context,
      base::Bind(&ExtendedAuthenticatorImpl::DoAuthenticateToCheck,
                 this,
                 success_callback));
}

void ExtendedAuthenticatorImpl::AddKey(const UserContext& context,
                                       const cryptohome::KeyDefinition& key,
                                       bool clobber_if_exists,
                                       const base::Closure& success_callback) {
  TransformKeyIfNeeded(
      context, base::Bind(&ExtendedAuthenticatorImpl::DoAddKey, this, key,
                          clobber_if_exists, success_callback));
}

void ExtendedAuthenticatorImpl::UpdateKeyAuthorized(
    const UserContext& context,
    const cryptohome::KeyDefinition& key,
    const std::string& signature,
    const base::Closure& success_callback) {
  TransformKeyIfNeeded(
      context,
      base::Bind(&ExtendedAuthenticatorImpl::DoUpdateKeyAuthorized,
                 this,
                 key,
                 signature,
                 success_callback));
}

void ExtendedAuthenticatorImpl::RemoveKey(const UserContext& context,
                                      const std::string& key_to_remove,
                                      const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticatorImpl::DoRemoveKey,
                                  this,
                                  key_to_remove,
                                  success_callback));
}

void ExtendedAuthenticatorImpl::TransformKeyIfNeeded(
    const UserContext& user_context,
    const ContextCallback& callback) {
  if (user_context.GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN) {
    callback.Run(user_context);
    return;
  }

  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(
        base::Bind(&ExtendedAuthenticatorImpl::TransformKeyIfNeeded,
                   this,
                   user_context,
                   callback));
    return;
  }

  UserContext transformed_context = user_context;
  transformed_context.GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                          system_salt_);
  callback.Run(transformed_context);
}

ExtendedAuthenticatorImpl::~ExtendedAuthenticatorImpl() = default;

void ExtendedAuthenticatorImpl::OnSaltObtained(const std::string& system_salt) {
  salt_obtained_ = true;
  system_salt_ = system_salt;
  for (std::vector<base::Closure>::const_iterator it =
           system_salt_callbacks_.begin();
       it != system_salt_callbacks_.end();
       ++it) {
    it->Run();
  }
  system_salt_callbacks_.clear();
}

void ExtendedAuthenticatorImpl::DoAuthenticateToMount(
    const ResultCallback& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("MountEx");
  const Key* const key = user_context.GetKey();
  CryptohomeClient::Get()->MountEx(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(key->GetLabel(), key->GetSecret()),
      cryptohome::MountRequest(),
      base::BindOnce(&ExtendedAuthenticatorImpl::OnMountComplete, this,
                     "MountEx", user_context, success_callback));
}

void ExtendedAuthenticatorImpl::DoAuthenticateToCheck(
    const base::Closure& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("CheckKeyEx");
  cryptohome::HomedirMethods::GetInstance()->CheckKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequestFromKeyDef(
          cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
              user_context)),
      cryptohome::CheckKeyRequest(),
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                 "CheckKeyEx", user_context, success_callback));
}

void ExtendedAuthenticatorImpl::DoAddKey(const cryptohome::KeyDefinition& key,
                                         bool clobber_if_exists,
                                         const base::Closure& success_callback,
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
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                 "AddKeyEx", user_context, success_callback));
}

void ExtendedAuthenticatorImpl::DoUpdateKeyAuthorized(
    const cryptohome::KeyDefinition& key,
    const std::string& signature,
    const base::Closure& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("UpdateKeyAuthorized");

  const Key* const auth_key = user_context.GetKey();
  cryptohome::UpdateKeyRequest request;
  cryptohome::KeyDefinitionToKey(key, request.mutable_changes());
  request.set_authorization_signature(signature);
  cryptohome::HomedirMethods::GetInstance()->UpdateKeyEx(
      cryptohome::Identification(user_context.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(auth_key->GetLabel(),
                                             auth_key->GetSecret()),
      request,
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                 "UpdateKeyAuthorized", user_context, success_callback));
}

void ExtendedAuthenticatorImpl::DoRemoveKey(const std::string& key_to_remove,
                                        const base::Closure& success_callback,
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
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete, this,
                 "RemoveKeyEx", user_context, success_callback));
}

void ExtendedAuthenticatorImpl::OnMountComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    const ResultCallback& success_callback,
    base::Optional<cryptohome::BaseReply> reply) {
  cryptohome::MountError return_code =
      cryptohome::MountExReplyToMountError(reply);
  RecordEndMarker(time_marker);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    const std::string& mount_hash =
        cryptohome::MountExReplyToMountHash(reply.value());
    if (!success_callback.is_null())
      success_callback.Run(mount_hash);
    if (old_consumer_) {
      UserContext copy = user_context;
      copy.SetUserIDHash(mount_hash);
      old_consumer_->OnAuthSuccess(copy);
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

  if (consumer_)
    consumer_->OnAuthenticationFailure(state);

  if (old_consumer_) {
    AuthFailure failure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
    old_consumer_->OnAuthFailure(failure);
  }
}

void ExtendedAuthenticatorImpl::OnOperationComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    const base::Closure& success_callback,
    bool success,
    cryptohome::MountError return_code) {
  RecordEndMarker(time_marker);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    if (!success_callback.is_null())
      success_callback.Run();
    if (old_consumer_)
      old_consumer_->OnAuthSuccess(user_context);
    return;
  }

  LOG(ERROR) << "Supervised user cryptohome error, code: " << return_code;

  AuthState state = FAILED_MOUNT;

  if (return_code == cryptohome::MOUNT_ERROR_TPM_COMM_ERROR ||
      return_code == cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK ||
      return_code == cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    state = FAILED_TPM;
  }

  if (return_code == cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST)
    state = NO_MOUNT;

  if (consumer_)
    consumer_->OnAuthenticationFailure(state);

  if (old_consumer_) {
    AuthFailure failure(AuthFailure::UNLOCK_FAILED);
    old_consumer_->OnAuthFailure(failure);
  }
}

}  // namespace chromeos
