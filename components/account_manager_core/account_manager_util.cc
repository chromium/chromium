// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_util.h"

#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace account_manager {

namespace cm = crosapi::mojom;

namespace {

GoogleServiceAuthError::InvalidGaiaCredentialsReason
FromMojoInvalidGaiaCredentialsReason(
    crosapi::mojom::GoogleServiceAuthError::InvalidGaiaCredentialsReason
        mojo_reason) {
  switch (mojo_reason) {
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::kUnknown:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN;
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        kCredentialsRejectedByServer:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_SERVER;
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        kCredentialsRejectedByClient:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_CLIENT;
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        kCredentialsMissing:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_MISSING;
    default:
      LOG(WARNING) << "Unknown "
                      "crosapi::mojom::GoogleServiceAuthError::"
                      "InvalidGaiaCredentialsReason: "
                   << mojo_reason;
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN;
  }
}

crosapi::mojom::GoogleServiceAuthError::InvalidGaiaCredentialsReason
ToMojoInvalidGaiaCredentialsReason(
    GoogleServiceAuthError::InvalidGaiaCredentialsReason reason) {
  switch (reason) {
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::kUnknown;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        CREDENTIALS_REJECTED_BY_SERVER:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          kCredentialsRejectedByServer;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        CREDENTIALS_REJECTED_BY_CLIENT:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          kCredentialsRejectedByClient;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        CREDENTIALS_MISSING:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          kCredentialsMissing;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::NUM_REASONS:
      NOTREACHED();
  }
}

GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason
FromMojoScopeLimitedUnrecoverableErrorReason(
    crosapi::mojom::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason
        mojo_reason) {
  switch (mojo_reason) {
    case cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kInvalidGrantRaptError:
      return GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kInvalidGrantRaptError;
    case cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kInvalidScope:
      return GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kInvalidScope;
    case cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kRestrictedClient:
      return GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kRestrictedClient;
    case cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kAdminPolicyEnforced:
      return GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kAdminPolicyEnforced;
    case cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kRemoteConsentResolutionRequired:
      return GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kRemoteConsentResolutionRequired;
    case cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kAccessDenied:
      return GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kAccessDenied;
  }
}

crosapi::mojom::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason
ToMojoScopeLimitedUnrecoverableErrorReason(
    GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason reason) {
  switch (reason) {
    case GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kInvalidGrantRaptError:
      return cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kInvalidGrantRaptError;
    case GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kInvalidScope:
      return cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kInvalidScope;
    case GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kRestrictedClient:
      return cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kRestrictedClient;
    case GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kAdminPolicyEnforced:
      return cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kAdminPolicyEnforced;
    case GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kRemoteConsentResolutionRequired:
      return cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kRemoteConsentResolutionRequired;
    case GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
        kAccessDenied:
      return cm::GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
          kAccessDenied;
  }
}

crosapi::mojom::GoogleServiceAuthError::State ToMojoGoogleServiceAuthErrorState(
    GoogleServiceAuthError::State state) {
  switch (state) {
    case GoogleServiceAuthError::State::NONE:
      return cm::GoogleServiceAuthError::State::kNone;
    case GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS:
      return cm::GoogleServiceAuthError::State::kInvalidGaiaCredentials;
    case GoogleServiceAuthError::State::ACCOUNT_NOT_FOUND:
      return cm::GoogleServiceAuthError::State::kAccountNotFound;
    case GoogleServiceAuthError::State::CONNECTION_FAILED:
      return cm::GoogleServiceAuthError::State::kConnectionFailed;
    case GoogleServiceAuthError::State::SERVICE_UNAVAILABLE:
      return cm::GoogleServiceAuthError::State::kServiceUnavailable;
    case GoogleServiceAuthError::State::REQUEST_CANCELED:
      return cm::GoogleServiceAuthError::State::kRequestCanceled;
    case GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE:
      return cm::GoogleServiceAuthError::State::kUnexpectedServiceResponse;
    case GoogleServiceAuthError::State::SERVICE_ERROR:
      return cm::GoogleServiceAuthError::State::kServiceError;
    case GoogleServiceAuthError::State::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
      return cm::GoogleServiceAuthError::State::kScopeLimitedUnrecoverableError;
    case GoogleServiceAuthError::State::CHALLENGE_RESPONSE_REQUIRED:
      return cm::GoogleServiceAuthError::State::kChallengeResponseRequired;
    case GoogleServiceAuthError::State::DEVICE_MANAGEMENT_ERROR:
      NOTREACHED();
    case GoogleServiceAuthError::State::NUM_STATES:
      NOTREACHED();
  }
}

}  // namespace

std::optional<account_manager::Account> FromMojoAccount(
    const crosapi::mojom::AccountPtr& mojom_account) {
  if (mojom_account.is_null()) {
    return std::nullopt;
  }

  const std::optional<account_manager::AccountKey> account_key =
      FromMojoAccountKey(mojom_account->key);
  if (!account_key.has_value()) {
    return std::nullopt;
  }

  account_manager::Account account{account_key.value(),
                                   mojom_account->raw_email};
  return account;
}

crosapi::mojom::AccountPtr ToMojoAccount(
    const account_manager::Account& account) {
  crosapi::mojom::AccountPtr mojom_account = crosapi::mojom::Account::New();
  mojom_account->key = ToMojoAccountKey(account.key);
  mojom_account->raw_email = account.raw_email;
  return mojom_account;
}

std::optional<account_manager::AccountKey> FromMojoAccountKey(
    const crosapi::mojom::AccountKeyPtr& mojom_account_key) {
  if (mojom_account_key.is_null()) {
    return std::nullopt;
  }

  const std::optional<account_manager::AccountType> account_type =
      FromMojoAccountType(mojom_account_key->account_type);
  if (!account_type.has_value()) {
    return std::nullopt;
  }
  if (mojom_account_key->id.empty()) {
    return std::nullopt;
  }

  return account_manager::AccountKey(mojom_account_key->id,
                                     account_type.value());
}

crosapi::mojom::AccountKeyPtr ToMojoAccountKey(
    const account_manager::AccountKey& account_key) {
  crosapi::mojom::AccountKeyPtr mojom_account_key =
      crosapi::mojom::AccountKey::New();
  mojom_account_key->id = account_key.id();
  mojom_account_key->account_type =
      ToMojoAccountType(account_key.account_type());
  return mojom_account_key;
}

std::optional<account_manager::AccountType> FromMojoAccountType(
    const crosapi::mojom::AccountType& account_type) {
  switch (account_type) {
    case crosapi::mojom::AccountType::kGaia:
      static_assert(static_cast<int>(crosapi::mojom::AccountType::kGaia) ==
                        static_cast<int>(account_manager::AccountType::kGaia),
                    "Underlying enum values must match");
      return account_manager::AccountType::kGaia;
    default:
      // Don't consider this as as error to preserve forwards compatibility with
      // lacros.
      LOG(WARNING) << "Unknown account type: " << account_type;
      return std::nullopt;
  }
}

crosapi::mojom::AccountType ToMojoAccountType(
    const account_manager::AccountType& account_type) {
  // Currently, we only support `kGaia` account type. Should a new type be added
  // in the future, consider removing the `CHECK_EQ()` below and handling the
  // new type accordingly.
  CHECK_EQ(account_type, account_manager::AccountType::kGaia);

  return crosapi::mojom::AccountType::kGaia;
}

std::optional<GoogleServiceAuthError> FromMojoGoogleServiceAuthError(
    const crosapi::mojom::GoogleServiceAuthErrorPtr& mojo_error) {
  if (mojo_error.is_null()) {
    return std::nullopt;
  }

  switch (mojo_error->state) {
    case cm::GoogleServiceAuthError::State::kNone:
      return GoogleServiceAuthError::AuthErrorNone();
    case cm::GoogleServiceAuthError::State::kInvalidGaiaCredentials:
      return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          FromMojoInvalidGaiaCredentialsReason(
              mojo_error->invalid_gaia_credentials_reason));
    case cm::GoogleServiceAuthError::State::kConnectionFailed:
      return GoogleServiceAuthError::FromConnectionError(
          mojo_error->network_error);
    case cm::GoogleServiceAuthError::State::kServiceError:
      return GoogleServiceAuthError::FromServiceError(
          mojo_error->error_message);
    case cm::GoogleServiceAuthError::State::kUnexpectedServiceResponse:
      return GoogleServiceAuthError::FromUnexpectedServiceResponse(
          mojo_error->error_message);
    case cm::GoogleServiceAuthError::State::kAccountNotFound:
      return GoogleServiceAuthError::CreateAccountNotFound();
    case cm::GoogleServiceAuthError::State::kServiceUnavailable:
      return GoogleServiceAuthError::FromServiceUnavailable("");
    case cm::GoogleServiceAuthError::State::kRequestCanceled:
      return GoogleServiceAuthError::CreateRequestCanceled();
    case cm::GoogleServiceAuthError::State::kScopeLimitedUnrecoverableError:
      return GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
          FromMojoScopeLimitedUnrecoverableErrorReason(
              mojo_error->scope_limited_unrecoverable_error_reason));
    case cm::GoogleServiceAuthError::State::kChallengeResponseRequired:
      return GoogleServiceAuthError::FromTokenBindingChallenge(
          mojo_error->token_binding_challenge.value_or(
              "MISSING_CHALLENGE_FROM_CROSAPI_MOJOM"));
    default:
      LOG(WARNING) << "Unknown crosapi::mojom::GoogleServiceAuthError::State: "
                   << mojo_error->state;
      return std::nullopt;
  }
}

crosapi::mojom::GoogleServiceAuthErrorPtr ToMojoGoogleServiceAuthError(
    GoogleServiceAuthError error) {
  crosapi::mojom::GoogleServiceAuthErrorPtr mojo_result =
      crosapi::mojom::GoogleServiceAuthError::New();
  mojo_result->error_message = error.error_message();
  if (error.state() == GoogleServiceAuthError::State::CONNECTION_FAILED) {
    mojo_result->network_error = error.GetNetworkError();
  }
  if (error.state() ==
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS) {
    mojo_result->invalid_gaia_credentials_reason =
        ToMojoInvalidGaiaCredentialsReason(
            error.GetInvalidGaiaCredentialsReason());
  }
  if (error.state() ==
      GoogleServiceAuthError::State::SCOPE_LIMITED_UNRECOVERABLE_ERROR) {
    mojo_result->scope_limited_unrecoverable_error_reason =
        ToMojoScopeLimitedUnrecoverableErrorReason(
            error.GetScopeLimitedUnrecoverableErrorReason());
  }
  if (error.state() ==
      GoogleServiceAuthError::State::CHALLENGE_RESPONSE_REQUIRED) {
    mojo_result->token_binding_challenge = error.GetTokenBindingChallenge();
  }
  mojo_result->state = ToMojoGoogleServiceAuthErrorState(error.state());
  return mojo_result;
}

}  // namespace account_manager
