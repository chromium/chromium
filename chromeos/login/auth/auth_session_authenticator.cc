// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/auth_session_authenticator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/cryptohome/userdataauth_util.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/login/auth/user_context.h"
#include "components/device_event_log/device_event_log.h"

namespace chromeos {

namespace {

// -- TransformCryotohomeKeyCallback implementations
void TransformToLabeledKey(const UserContext& context, cryptohome::Key* key) {
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(context), key);
}

void TransformToWildcardKey(const UserContext& context, cryptohome::Key* key) {
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
          context),
      key);
}

// -- ConfigureAuthSessionCallback implementations

user_data_auth::StartAuthSessionRequest ConfigureRegularSession(
    const UserContext& context,
    user_data_auth::StartAuthSessionRequest request) {
  // TODO: Once ephemeral policy would be handled on Chrome side update flags.
  request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_NONE);
  return request;
}

// -- ConfigureMountCallback implementations

user_data_auth::MountRequest ConfigureGenericMount(
    const UserContext& context,
    user_data_auth::MountRequest request) {
  if (context.IsForcingDircrypto())
    request.set_force_dircrypto_if_available(true);
  return request;
}

user_data_auth::MountRequest ConfigureCreateMount(
    const UserContext& context,
    user_data_auth::MountRequest request) {
  // Explicitly mark request as a create request. Do not add any keys.
  request.mutable_create();
  return ConfigureGenericMount(context, std::move(request));
}

// -- KeyHashingCallback implementations

void TransformGaiaPasswordWithSalt(
    std::unique_ptr<UserContext> user_context,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> callback,
    const std::string& system_salt) {
  DCHECK_EQ(user_context->GetKey()->GetKeyType(), Key::KEY_TYPE_PASSWORD_PLAIN);
  user_context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                    system_salt);
  std::move(callback).Run(std::move(user_context));
}

void HashPassword(
    std::unique_ptr<UserContext> context,
    const user_data_auth::StartAuthSessionReply& reply,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> consumer) {
  if (context->GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN) {
    std::move(consumer).Run(std::move(context));
    return;
  }

  DCHECK(!context->IsUsingPin());
  // TODO(antrim): use key metadata if necessary.
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      TransformGaiaPasswordWithSalt, std::move(context), std::move(consumer)));
}

}  // namespace

AuthSessionAuthenticator::AuthSessionAuthenticator(
    AuthStatusConsumer* consumer,
    std::unique_ptr<SafeModeDelegate> safe_mode_delegate)
    : Authenticator(consumer),
      safe_mode_delegate_(std::move(safe_mode_delegate)) {}

AuthSessionAuthenticator::~AuthSessionAuthenticator() = default;

// Completes online authentication:
// *  User is likely to be new
// *  Provided password is assumed to be just verified by online flow
void AuthSessionAuthenticator::CompleteLogin(const UserContext& user_context) {
  DCHECK(user_context.GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context.GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context.GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  PrepareForNewAttempt("CompleteLogin", "Regular user after online sign-in");

  // For now we don't support empty passwords:
  if (user_context.GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (user_context.GetKey()->GetSecret().empty()) {
      NOTIMPLEMENTED();
      if (consumer_)
        consumer_->OnAuthFailure(
            AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
      return;
    }
  }

  std::unique_ptr<UserContext> context =
      std::make_unique<UserContext>(user_context);

  // (1) Initialize AuthSession & transform keys
  //   (1.1) For regular users
  //   (1.2) Key is a password
  //   (2) For existing users:
  //     (3) Authenticate AuthSession
  //       (3.1) Password mismatch means that password changed
  //     (4) Mount directory
  //         (4.1) with regular flags
  //     (5) (Safe mode) Check ownership
  //     (#) Notify success
  //   (6) For new users:
  //     (7) (Safe mode) fail, as this user can not be owner
  //     (8) Add user key
  //     (9) Authenticate session with same key
  //     (10) Mount home directory
  //        (10.1) with create request
  //     (#) Notify success
  // (*) Errors are notified as COULD_NOT_MOUNT_CRYPTOHOME

  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /* default_error */ AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  // (#)
  auto success_split = base::SplitOnceCallback(
      base::BindOnce(&AuthSessionAuthenticator::NotifyAuthSuccess,
                     weak_factory_.GetWeakPtr()));
  // (10.1)
  ConfigureMountCallback mount_create_cfg =
      base::BindOnce(&ConfigureCreateMount);
  // (10)
  ContextCallback mount_create = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /* error_handler */ base::BindOnce(error_handler_repeating),
      /* configurator */ std::move(mount_create_cfg),
      /* continunation */ std::move(success_split.first));
  // (9)
  ContextCallback authenticate_same_key = base::BindOnce(
      &AuthSessionAuthenticator::AuthenticateSessionGeneric,
      weak_factory_.GetWeakPtr(),
      /* error_handler */ base::BindOnce(error_handler_repeating),
      /* key_transformer */ base::BindOnce(&TransformToWildcardKey),
      /* continuation */ std::move(mount_create));

  // (8)
  NewUserAuthSessionCallback new_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::AddInitialCredentialsGeneric,
      weak_factory_.GetWeakPtr(),
      /* error_handler */ base::BindOnce(error_handler_repeating),
      /* key_transformer */ base::BindOnce(&TransformToLabeledKey),
      /* continuation */ std::move(authenticate_same_key));
  // TODO(antrim): implement (7)
  // TODO(antrim): implement (5)
  // (4.1)
  ConfigureMountCallback mount_regular_cfg =
      base::BindOnce(&ConfigureGenericMount);
  // (4)
  ContextCallback mount_existing = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /* error_handler */ base::BindOnce(error_handler_repeating),
      /* configurator */ std::move(mount_regular_cfg),
      /* continuation */ std::move(success_split.second));
  // (3.1)
  ErrorHandlingCallback auth_error_handler =
      base::BindOnce(&AuthSessionAuthenticator::
                         ExistingUserPasswordAuthenticationErrorHandling,
                     weak_factory_.GetWeakPtr(),
                     /* fallback */ base::BindOnce(error_handler_repeating),
                     /* verified password */ true);
  // (3)
  ExistingUserAuthSessionCallback existing_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::AuthenticateSessionGeneric,
      weak_factory_.GetWeakPtr(),
      /* error_handler */ std::move(auth_error_handler),
      /* key_transformer */ base::BindOnce(&TransformToWildcardKey),
      /* continuation */ std::move(mount_existing));

  // (1.2)
  auto password_transformer = base::BindOnce(&HashPassword);
  // (1.1)
  auto regular_session = base::BindOnce(&ConfigureRegularSession);
  // (1)
  CreateAuthSessionGeneric(
      "RegularUser", base::BindOnce(error_handler_repeating),
      /* configurator */ std::move(regular_session),
      /* password_hasher */ std::move(password_transformer),
      /* new_user_flow (6) */ std::move(new_user_flow),
      /* existing_user_flow (2) */ std::move(existing_user_flow),
      std::move(context));
}

void AuthSessionAuthenticator::AuthenticateToLogin(
    const UserContext& user_context) {
  DCHECK(user_context.GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context.GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context.GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::LoginOffTheRecord() {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::LoginAsPublicSession(
    const UserContext& user_context) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::LoginAsKioskAccount(
    const AccountId& app_account_id) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::LoginAsArcKioskAccount(
    const AccountId& app_account_id) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::LoginAsWebKioskAccount(
    const AccountId& app_account_id) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::OnAuthSuccess() {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::OnAuthFailure(const AuthFailure& error) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::RecoverEncryptedData(
    const std::string& old_password) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::ResyncEncryptedData() {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::PrepareForNewAttempt(
    const std::string& method_id,
    const std::string& long_desc) {
  LOGIN_LOG(USER) << "Authentication attempt : " << long_desc;
  VLOG(1) << "AuthSessionAuthenticator::" << method_id;

  // Assume no ongoing authentication requests happen at the moment.
  DCHECK(!weak_factory_.HasWeakPtrs());
  // Clear all ongoing requests
  weak_factory_.InvalidateWeakPtrs();
}

void AuthSessionAuthenticator::ProcessCryptohomeError(
    AuthFailure::FailureReason default_error,
    std::unique_ptr<UserContext> user_context,
    user_data_auth::CryptohomeErrorCode error) {
  if (!consumer_)
    return;

  switch (error) {
    case user_data_auth::CRYPTOHOME_ERROR_NOT_SET:
      NOTREACHED() << "Should be called with an error";
      return;
    case user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND:
      consumer_->OnAuthFailure(AuthFailure(AuthFailure::MISSING_CRYPTOHOME));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_NOT_IMPLEMENTED:
    case user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT:
    case user_data_auth::CRYPTOHOME_TOKEN_SERIALIZATION_FAILED:
      // Fatal implementation errors
      break;
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_ERROR_INTERNAL:
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED:
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_DENIED:
      // Fingerprint errors
      break;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_QUOTA_EXCEEDED:
    case user_data_auth::CRYPTOHOME_ERROR_BACKING_STORE_FAILURE:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_GET_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_SET_FAILED:
      // Fatal system state errors
      break;
    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_MIGRATE_KEY_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED:

    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_LABEL_EXISTS:
    case user_data_auth::CRYPTOHOME_ERROR_UPDATE_SIGNATURE_INVALID:
      // Assumptions about key are not correct
      break;
    case user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN:
      // Auth session expired, might need to handle it separately later.
      break;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_COMM_ERROR:
    case user_data_auth::CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT:
      consumer_->OnAuthFailure(AuthFailure(AuthFailure::TPM_ERROR));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK:
      consumer_->OnAuthFailure(AuthFailure(AuthFailure::TPM_ERROR));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY:
      // Assumption about system state is not correct
      break;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION:
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE:
      NOTREACHED() << "Encryption migration should be handled separately";
      return;
    case user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED:
      NOTREACHED() << "Add credentials failure should be handled separately";
      return;
    case user_data_auth::CRYPTOHOME_ERROR_REMOVE_FAILED:
      consumer_->OnAuthFailure(AuthFailure(AuthFailure::DATA_REMOVAL_FAILED));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_UPDATE_REQUIRED:
      consumer_->OnAuthFailure(AuthFailure(AuthFailure::TPM_UPDATE_REQUIRED));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_VAULT_UNRECOVERABLE:
      consumer_->OnAuthFailure(
          AuthFailure(AuthFailure::UNRECOVERABLE_CRYPTOHOME));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_LOCKBOX_SIGNATURE_INVALID:
    case user_data_auth::CRYPTOHOME_ERROR_LOCKBOX_CANNOT_SIGN:
    case user_data_auth::CRYPTOHOME_ERROR_BOOT_ATTRIBUTE_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_BOOT_ATTRIBUTES_CANNOT_SIGN:
    case user_data_auth::CRYPTOHOME_ERROR_TPM_EK_NOT_AVAILABLE:
    case user_data_auth::CRYPTOHOME_ERROR_ATTESTATION_NOT_READY:
    case user_data_auth::CRYPTOHOME_ERROR_CANNOT_CONNECT_TO_CA:
    case user_data_auth::CRYPTOHOME_ERROR_CA_REFUSED_ENROLLMENT:
    case user_data_auth::CRYPTOHOME_ERROR_CA_REFUSED_CERTIFICATE:
    case user_data_auth::CRYPTOHOME_ERROR_INTERNAL_ATTESTATION_ERROR:
    case user_data_auth::
        CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_INVALID:
    case user_data_auth::
        CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_STORE:
    case user_data_auth::
        CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_REMOVE:
    case user_data_auth::CRYPTOHOME_ERROR_UPDATE_USER_ACTIVITY_TIMESTAMP_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_FAILED_TO_EXTEND_PCR:
    case user_data_auth::CRYPTOHOME_ERROR_FAILED_TO_READ_PCR:
    case user_data_auth::CRYPTOHOME_ERROR_PCR_ALREADY_EXTENDED:
    case user_data_auth::CRYPTOHOME_ERROR_FIDO_MAKE_CREDENTIAL_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_FIDO_GET_ASSERTION_FAILED:
      // Also fatal errors that should not be surfaced.
      NOTREACHED();
      return;
    case user_data_auth::CryptohomeErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case user_data_auth::CryptohomeErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_:
      // Ignored
      return;
  }
  consumer_->OnAuthFailure(AuthFailure(default_error));
}

void AuthSessionAuthenticator::CreateAuthSessionGeneric(
    const std::string& user_type,
    ErrorHandlingCallback error_handler,
    ConfigureAuthSessionCallback configurator,
    KeyHashingCallback key_hasher,
    NewUserAuthSessionCallback new_user_flow,
    ExistingUserAuthSessionCallback existing_user_flow,
    std::unique_ptr<UserContext> context) {
  user_data_auth::StartAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());

  request = std::move(configurator).Run(*context, std::move(request));

  UserDataAuthClient::StartAuthSessionCallback auth_session_callback =
      base::BindOnce(&AuthSessionAuthenticator::OnAuthSessionCreatedGeneric,
                     weak_factory_.GetWeakPtr(), user_type,
                     std::move(error_handler), std::move(key_hasher),
                     std::move(new_user_flow), std::move(existing_user_flow),
                     std::move(context));

  UserDataAuthClient::Get()->StartAuthSession(request,
                                              std::move(auth_session_callback));
}

void AuthSessionAuthenticator::OnAuthSessionCreatedGeneric(
    const std::string& user_type,
    ErrorHandlingCallback error_handler,
    KeyHashingCallback key_hasher,
    NewUserAuthSessionCallback new_user_flow,
    ExistingUserAuthSessionCallback existing_user_flow,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::StartAuthSessionReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::AuthSessionCreated " << user_type;

  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "StartAuthSession for " << user_type
                     << " failed with error " << error;
    std::move(error_handler).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  context->SetAuthSessionId(reply->auth_session_id());

  ContextCallback consumer;
  if (reply->user_exists()) {
    consumer = std::move(existing_user_flow);
  } else {
    LOGIN_LOG(EVENT) << "User is new";
    consumer = std::move(new_user_flow);
  }
  std::move(key_hasher)
      .Run(std::move(context), reply.value(), std::move(consumer));
}

void AuthSessionAuthenticator::AuthenticateSessionGeneric(
    ErrorHandlingCallback auth_error_callback,
    TransformCryotohomeKeyCallback transformer,
    ContextCallback continuation,
    std::unique_ptr<UserContext> transformed_context) {
  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(transformed_context->GetAuthSessionId());
  std::move(transformer)
      .Run(*transformed_context,
           request.mutable_authorization()->mutable_key());
  auto authentication_callback =
      base::BindOnce(&AuthSessionAuthenticator::OnAuthenticateSessionGeneric,
                     weak_factory_.GetWeakPtr(), std::move(auth_error_callback),
                     std::move(continuation), std::move(transformed_context));

  UserDataAuthClient::Get()->AuthenticateAuthSession(
      request, std::move(authentication_callback));
}

void AuthSessionAuthenticator::OnAuthenticateSessionGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnAuthenticateSession";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AuthenticateSession failed with error " << error;
    std::move(error_handler).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());

  // No 2FA support yet.
  DCHECK(reply->authenticated());

  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::AddInitialCredentialsGeneric(
    ErrorHandlingCallback error_handler,
    TransformCryotohomeKeyCallback transformer,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  user_data_auth::AddCredentialsRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_add_more_credentials(false);

  std::move(transformer)
      .Run(*context, request.mutable_authorization()->mutable_key());

  UserDataAuthClient::AddCredentialsCallback add_credentials_callback =
      base::BindOnce(&AuthSessionAuthenticator::OnAddInitialCredentialsGeneric,
                     weak_factory_.GetWeakPtr(), std::move(error_handler),
                     std::move(continuation), std::move(context));

  UserDataAuthClient::Get()->AddCredentials(
      request, std::move(add_credentials_callback));
}

void AuthSessionAuthenticator::OnAddInitialCredentialsGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::AddCredentialsReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnAddInitialCredentials";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AddCredentials(initial) failed with error " << error;
    if (error == user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED) {
      // TODO(antrim): implement correct handling
      NOTIMPLEMENTED();
      return;
    }
    std::move(error_handler).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::MountGeneric(
    ErrorHandlingCallback error_handler,
    ConfigureMountCallback configurator,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  VLOG(1) << "AuthSessionAuthenticator::Mount";
  user_data_auth::MountRequest mount;

  mount.set_auth_session_id(context->GetAuthSessionId());

  if (configurator)
    mount = std::move(configurator).Run(*context, std::move(mount));

  ErrorHandlingCallback mount_error_handler =
      base::BindOnce(&AuthSessionAuthenticator::MountErrorHandling,
                     weak_factory_.GetWeakPtr(), std::move(error_handler));

  UserDataAuthClient::Get()->Mount(
      mount,
      base::BindOnce(&AuthSessionAuthenticator::OnMountGeneric,
                     weak_factory_.GetWeakPtr(), std::move(mount_error_handler),
                     std::move(continuation), std::move(context)));
}

void AuthSessionAuthenticator::OnMountGeneric(
    ErrorHandlingCallback error_callback,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::MountReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnMount";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Mount failed with error " << error;
    std::move(error_callback).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  context->SetUserIDHash(reply->sanitized_username());

  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::ExistingUserPasswordAuthenticationErrorHandling(
    ErrorHandlingCallback fallback,
    bool verified_password,
    std::unique_ptr<UserContext> context,
    user_data_auth::CryptohomeErrorCode error) {
  if ((error == user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED) &&
      verified_password) {
    LOGIN_LOG(EVENT) << "Password change detected";
    if (!consumer_)
      return;
    consumer_->OnPasswordChangeDetected(*context);
    return;
  }
  std::move(fallback).Run(std::move(context), error);
}

void AuthSessionAuthenticator::MountErrorHandling(
    ErrorHandlingCallback fallback,
    std::unique_ptr<UserContext> context,
    user_data_auth::CryptohomeErrorCode error) {
  if (error == user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION ||
      error == user_data_auth::
                   CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE) {
    LOGIN_LOG(EVENT) << "Old encryption detected";
    if (!consumer_)
      return;
    consumer_->OnOldEncryptionDetected(
        *context, error ==
                      user_data_auth::
                          CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE);
    return;
  }
  std::move(fallback).Run(std::move(context), error);
}

void AuthSessionAuthenticator::NotifyAuthSuccess(
    std::unique_ptr<UserContext> context) {
  if (consumer_)
    consumer_->OnAuthSuccess(*context);
}

}  // namespace chromeos
