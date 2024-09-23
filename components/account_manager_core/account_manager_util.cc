// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_util.h"

#include <optional>

#include "base/notreached.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_upsertion_result.h"
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
      NOTREACHED_IN_MIGRATION();
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::kUnknown;
  }
}

crosapi::mojom::GoogleServiceAuthError::State ToMojoGoogleServiceAuthErrorState(
    GoogleServiceAuthError::State state) {
  switch (state) {
    case GoogleServiceAuthError::State::NONE:
      return cm::GoogleServiceAuthError::State::kNone;
    case GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS:
      return cm::GoogleServiceAuthError::State::kInvalidGaiaCredentials;
    case GoogleServiceAuthError::State::USER_NOT_SIGNED_UP:
      return cm::GoogleServiceAuthError::State::kUserNotSignedUp;
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
    case GoogleServiceAuthError::State::NUM_STATES:
      NOTREACHED_IN_MIGRATION();
      return cm::GoogleServiceAuthError::State::kNone;
  }
}

std::optional<account_manager::AccountUpsertionResult::Status>
FromMojoAccountAdditionStatus(
    crosapi::mojom::AccountUpsertionResult::Status mojo_status) {
  switch (mojo_status) {
    case cm::AccountUpsertionResult::Status::kSuccess:
      return account_manager::AccountUpsertionResult::Status::kSuccess;
    case cm::AccountUpsertionResult::Status::kAlreadyInProgress:
      return account_manager::AccountUpsertionResult::Status::
          kAlreadyInProgress;
    case cm::AccountUpsertionResult::Status::kCancelledByUser:
      return account_manager::AccountUpsertionResult::Status::kCancelledByUser;
    case cm::AccountUpsertionResult::Status::kNetworkError:
      return account_manager::AccountUpsertionResult::Status::kNetworkError;
    case cm::AccountUpsertionResult::Status::kUnexpectedResponse:
      return account_manager::AccountUpsertionResult::Status::
          kUnexpectedResponse;
    case cm::AccountUpsertionResult::Status::kBlockedByPolicy:
      return account_manager::AccountUpsertionResult::Status::kBlockedByPolicy;
    default:
      LOG(WARNING) << "Unknown crosapi::mojom::AccountUpsertionResult::Status: "
                   << mojo_status;
      return std::nullopt;
  }
}

crosapi::mojom::AccountUpsertionResult::Status ToMojoAccountAdditionStatus(
    account_manager::AccountUpsertionResult::Status status) {
  switch (status) {
    case account_manager::AccountUpsertionResult::Status::kSuccess:
      return cm::AccountUpsertionResult::Status::kSuccess;
    case account_manager::AccountUpsertionResult::Status::kAlreadyInProgress:
      return cm::AccountUpsertionResult::Status::kAlreadyInProgress;
    case account_manager::AccountUpsertionResult::Status::kCancelledByUser:
      return cm::AccountUpsertionResult::Status::kCancelledByUser;
    case account_manager::AccountUpsertionResult::Status::kNetworkError:
      return cm::AccountUpsertionResult::Status::kNetworkError;
    case account_manager::AccountUpsertionResult::Status::kUnexpectedResponse:
      return cm::AccountUpsertionResult::Status::kUnexpectedResponse;
    case account_manager::AccountUpsertionResult::Status::kBlockedByPolicy:
      return cm::AccountUpsertionResult::Status::kBlockedByPolicy;
    case account_manager::AccountUpsertionResult::Status::
        kMojoRemoteDisconnected:
    case account_manager::AccountUpsertionResult::Status::
        kIncompatibleMojoVersions:
      // `kMojoRemoteDisconnected` and `kIncompatibleMojoVersions` are generated
      // entirely on the remote side when the receiver can't even be reached.
      // They do not have any Mojo equivalent since they are never passed over
      // the wire in the first place.
      NOTREACHED_IN_MIGRATION()
          << "These statuses should not be passed over the wire";
      // Return something to make the compiler happy. This should never happen
      // in production.
      return cm::AccountUpsertionResult::Status::kUnexpectedResponse;
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
  if (!account_key.has_value())
    return std::nullopt;

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
  if (!account_type.has_value())
    return std::nullopt;
  if (mojom_account_key->id.empty())
    return std::nullopt;

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
    case crosapi::mojom::AccountType::kActiveDirectory:
      static_assert(
          static_cast<int>(crosapi::mojom::AccountType::kActiveDirectory) ==
              static_cast<int>(account_manager::AccountType::kActiveDirectory),
          "Underlying enum values must match");
      return account_manager::AccountType::kActiveDirectory;
    default:
      // Don't consider this as as error to preserve forwards compatibility with
      // lacros.
      LOG(WARNING) << "Unknown account type: " << account_type;
      return std::nullopt;
  }
}

crosapi::mojom::AccountType ToMojoAccountType(
    const account_manager::AccountType& account_type) {
  switch (account_type) {
    case account_manager::AccountType::kGaia:
      return crosapi::mojom::AccountType::kGaia;
    case account_manager::AccountType::kActiveDirectory:
      return crosapi::mojom::AccountType::kActiveDirectory;
  }
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
    case cm::GoogleServiceAuthError::State::kUserNotSignedUp:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP);
    case cm::GoogleServiceAuthError::State::kServiceUnavailable:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);
    case cm::GoogleServiceAuthError::State::kRequestCanceled:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::State::REQUEST_CANCELED);
    case cm::GoogleServiceAuthError::State::kScopeLimitedUnrecoverableError:
      return GoogleServiceAuthError::FromScopeLimitedUnrecoverableError(
          mojo_error->error_message);
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
    mojo_result->network_error = error.network_error();
  }
  if (error.state() ==
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS) {
    mojo_result->invalid_gaia_credentials_reason =
        ToMojoInvalidGaiaCredentialsReason(
            error.GetInvalidGaiaCredentialsReason());
  }
  if (error.state() ==
      GoogleServiceAuthError::State::CHALLENGE_RESPONSE_REQUIRED) {
    mojo_result->token_binding_challenge = error.GetTokenBindingChallenge();
  }
  mojo_result->state = ToMojoGoogleServiceAuthErrorState(error.state());
  return mojo_result;
}

std::optional<account_manager::AccountUpsertionResult>
FromMojoAccountUpsertionResult(
    const crosapi::mojom::AccountUpsertionResultPtr& mojo_result) {
  if (mojo_result.is_null()) {
    return std::nullopt;
  }

  std::optional<account_manager::AccountUpsertionResult::Status> status =
      FromMojoAccountAdditionStatus(mojo_result->status);
  if (!status.has_value())
    return std::nullopt;

  switch (status.value()) {
    case account_manager::AccountUpsertionResult::Status::kSuccess: {
      std::optional<account_manager::Account> account =
          FromMojoAccount(mojo_result->account);
      if (!account.has_value())
        return std::nullopt;
      return account_manager::AccountUpsertionResult::FromAccount(
          account.value());
    }
    case account_manager::AccountUpsertionResult::Status::kNetworkError: {
      std::optional<GoogleServiceAuthError> net_error =
          FromMojoGoogleServiceAuthError(mojo_result->error);
      if (!net_error.has_value())
        return std::nullopt;
      return account_manager::AccountUpsertionResult::FromError(
          net_error.value());
    }
    case account_manager::AccountUpsertionResult::Status::kAlreadyInProgress:
    case account_manager::AccountUpsertionResult::Status::kCancelledByUser:
    case account_manager::AccountUpsertionResult::Status::kUnexpectedResponse:
    case account_manager::AccountUpsertionResult::Status::
        kMojoRemoteDisconnected:
    case account_manager::AccountUpsertionResult::Status::
        kIncompatibleMojoVersions:
      return account_manager::AccountUpsertionResult::FromStatus(
          status.value());
    case account_manager::AccountUpsertionResult::Status::kBlockedByPolicy:
      return account_manager::AccountUpsertionResult::FromStatus(
          status.value());
  }
}

crosapi::mojom::AccountUpsertionResultPtr ToMojoAccountUpsertionResult(
    account_manager::AccountUpsertionResult result) {
  crosapi::mojom::AccountUpsertionResultPtr mojo_result =
      crosapi::mojom::AccountUpsertionResult::New();
  mojo_result->status = ToMojoAccountAdditionStatus(result.status());
  if (result.account().has_value()) {
    mojo_result->account =
        account_manager::ToMojoAccount(result.account().value());
  }
  if (result.error().state() != GoogleServiceAuthError::NONE) {
    mojo_result->error = ToMojoGoogleServiceAuthError(result.error());
  }
  return mojo_result;
}

std::optional<account_manager::AccountAdditionOptions>
FromMojoAccountAdditionOptions(
    const crosapi::mojom::AccountAdditionOptionsPtr& mojo_options) {
  if (mojo_options.is_null()) {
    return std::nullopt;
  }

  account_manager::AccountAdditionOptions result;
  result.is_available_in_arc = mojo_options->is_available_in_arc;
  result.show_arc_availability_picker =
      mojo_options->show_arc_availability_picker;

  return result;
}

}  // namespace account_manager
