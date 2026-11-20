// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

#include "base/notreached.h"

namespace actor_login {

namespace {
Credential::Id GenerateCredentialId() {
  static Credential::Id::Generator generator;
  return generator.GenerateNextId();
}
}  // namespace

FederationDetail::FederationDetail() = default;

FederationDetail::FederationDetail(const FederationDetail&) = default;
FederationDetail::FederationDetail(FederationDetail&&) = default;
FederationDetail& FederationDetail::operator=(const FederationDetail&) =
    default;
FederationDetail& FederationDetail::operator=(FederationDetail&&) = default;

FederationDetail::~FederationDetail() = default;

Credential::Credential() : id(GenerateCredentialId()) {}

Credential::Credential(const Credential& other) = default;
Credential::Credential(Credential&& other) = default;

Credential& Credential::operator=(const Credential& credential) = default;
Credential& Credential::operator=(Credential&& credential) = default;

Credential::~Credential() = default;

optimization_guide::proto::
    ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome
    OutcomeEnumToProtoType(GetCredentialsOutcomeMqls outcome) {
  switch (outcome) {
    case GetCredentialsOutcomeMqls::kUnspecified:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_UNSPECIFIED;
    case GetCredentialsOutcomeMqls::kNoCredentials:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_CREDENTIALS;
    case GetCredentialsOutcomeMqls::kSignInFormExists:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_SIGN_IN_FORM_EXISTS;
    case GetCredentialsOutcomeMqls::kNoSignInForm:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_NO_SIGN_IN_FORM;
    case GetCredentialsOutcomeMqls::kFillingNotAllowed:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome_FILLING_NOT_ALLOWED;
  }
  NOTREACHED();
}

optimization_guide::proto::
    ActorLoginQuality_GetCredentialsDetails_PermissionDetails
    PermissionEnumToProtoType(PermissionDetailsMqls permission) {
  switch (permission) {
    case PermissionDetailsMqls::kUnknown:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_UNKNOWN;
    case PermissionDetailsMqls::kHasPermanentPermission:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_HAS_PERMANENT_PERMISSION;
    case PermissionDetailsMqls::kNoPermanentPermission:
      return optimization_guide::proto::
          ActorLoginQuality_GetCredentialsDetails_PermissionDetails_NO_PERMANENT_PERMISSION;
  }
  NOTREACHED();
}

optimization_guide::proto::
    ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome
    OutcomeEnumToProtoType(AttemptLoginOutcomeMqls outcome) {
  switch (outcome) {
    case AttemptLoginOutcomeMqls::kUnspecified:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_UNSPECIFIED;
    case AttemptLoginOutcomeMqls::kSuccess:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_SUCCESS;
    case AttemptLoginOutcomeMqls::kNoSignInForm:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_NO_SIGN_IN_FORM;
    case AttemptLoginOutcomeMqls::kInvalidCredential:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_INVALID_CREDENTIAL;
    case AttemptLoginOutcomeMqls::kNoFillableFields:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_NO_FILLABLE_FIELDS;
    case AttemptLoginOutcomeMqls::kDisallowedOrigin:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_DISALLOWED_ORIGIN;
    case AttemptLoginOutcomeMqls::kReauthRequired:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_REAUTH_REQUIRED;
    case AttemptLoginOutcomeMqls::kReauthFailed:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_REAUTH_FAILED;
    case AttemptLoginOutcomeMqls::kFederatedSuccess:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_SUCCESS;
    case AttemptLoginOutcomeMqls::kFederatedContinuation:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_CONTINUATION;
    case AttemptLoginOutcomeMqls::kFederatedAccountNotLoggedIn:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_ACCOUNT_NOT_LOGGED_IN;
    case AttemptLoginOutcomeMqls::kFederatedAccountIsSignUp:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_ACCOUNT_IS_SIGN_UP;
    case AttemptLoginOutcomeMqls::kFederatedAccountIsNotAvailable:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_ACCOUNT_IS_NOT_AVAILABLE;
    case AttemptLoginOutcomeMqls::kFederatedIdpReturnedError:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_IDP_RETURNED_ERROR;
    case AttemptLoginOutcomeMqls::kFederatedIdpNetworkError:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_IDP_NETWORK_ERROR;
    case AttemptLoginOutcomeMqls::kFederatedTokenRequestAborted:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_TOKEN_REQUEST_ABORTED;
    case AttemptLoginOutcomeMqls::kFederatedFrameNotActive:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_FRAME_NOT_ACTIVE;
    case AttemptLoginOutcomeMqls::kFederatedExpectedAccountNotPresent:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_EXPECTED_ACCOUNT_NOT_PRESENT;
    case AttemptLoginOutcomeMqls::kFederatedTimeout:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_TIMEOUT;
    case AttemptLoginOutcomeMqls::kFillingNotAllowed:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FILLING_NOT_ALLOWED;
    case AttemptLoginOutcomeMqls::kFillingInterruptedByPageChange:
      return optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FILLING_INTERRUPTED_BY_PAGE_CHANGE;
  }
  NOTREACHED();
}

actor::mojom::ActionResultCode LoginErrorToActorResult(
    ActorLoginError login_error) {
  switch (login_error) {
    case ActorLoginError::kServiceBusy:
      return actor::mojom::ActionResultCode::kLoginTooManyRequests;
    case ActorLoginError::kInvalidTabInterface:
      return actor::mojom::ActionResultCode::kTabWentAway;
    case ActorLoginError::kFillingNotAllowed:
      return actor::mojom::ActionResultCode::kLoginFillingNotAllowed;
    case ActorLoginError::kFeatureDisabled:
      return actor::mojom::ActionResultCode::kLoginFeatureDisabled;
  }
}

actor::mojom::ActionResultCode LoginResultToActorResult(
    LoginStatusResult login_result) {
  // TODO(crbug.com/427817201): Re-assess whether all success statuses should
  // map to kOk or if differentiation is needed.
  switch (login_result) {
    case LoginStatusResult::kSuccessUsernameAndPasswordFilled:
    case LoginStatusResult::kSuccessUsernameFilled:
    case LoginStatusResult::kSuccessPasswordFilled:
    case LoginStatusResult::kSuccessFederated:
      return actor::mojom::ActionResultCode::kOk;
    case LoginStatusResult::kErrorNoSigninForm:
      return actor::mojom::ActionResultCode::kLoginNotLoginPage;
    case LoginStatusResult::kErrorInvalidCredential:
      return actor::mojom::ActionResultCode::kLoginNoCredentialsAvailable;
    case LoginStatusResult::kErrorNoFillableFields:
      return actor::mojom::ActionResultCode::kLoginNoFillableFields;
    case LoginStatusResult::kErrorDeviceReauthRequired:
      return actor::mojom::ActionResultCode::kLoginDeviceReauthRequired;
    case LoginStatusResult::kErrorDeviceReauthFailed:
      return actor::mojom::ActionResultCode::kLoginDeviceReauthFailed;
    case LoginStatusResult::kErrorFederatedContinuation:
      return actor::mojom::ActionResultCode::kLoginFederatedContinuation;
    case LoginStatusResult::kErrorFederatedAccountNotLoggedIn:
      return actor::mojom::ActionResultCode::kLoginFederatedAccountNotLoggedIn;
    case LoginStatusResult::kErrorFederatedAccountIsSignUp:
      return actor::mojom::ActionResultCode::kLoginFederatedAccountIsSignUp;
    case LoginStatusResult::kErrorFederatedAccountNotAvailable:
      return actor::mojom::ActionResultCode::kLoginFederatedAccountNotAvailable;
    case LoginStatusResult::kErrorFederatedIdpReturnedError:
      return actor::mojom::ActionResultCode::kLoginFederatedIdpReturnedError;
    case LoginStatusResult::kErrorFederatedIdpNetworkError:
      return actor::mojom::ActionResultCode::kLoginFederatedIdpNetworkError;
    case LoginStatusResult::kErrorFederatedTokenRequestAborted:
      return actor::mojom::ActionResultCode::kLoginFederatedTokenRequestAborted;
    case LoginStatusResult::kErrorFederatedFrameNotActive:
      return actor::mojom::ActionResultCode::kLoginFederatedFrameNotActive;
    case LoginStatusResult::kErrorFederatedExpectedAccountNotPresent:
      return actor::mojom::ActionResultCode::
          kLoginFederatedExpectedAccountNotPresent;
    case LoginStatusResult::kErrorFederatedTimeout:
      return actor::mojom::ActionResultCode::kLoginFederatedTimeout;
    case LoginStatusResult::kRequiresButtonClick:
      // TODO(crbug.com/479505793): Consider adding a more specific error code.
      return actor::mojom::ActionResultCode::kArgumentsInvalid;
    case LoginStatusResult::kErrorPageChangedDuringFilling:
      return actor::mojom::ActionResultCode::kLoginPasswordFillingPageChanged;
  }
}

bool IsSuccessfulPasswordCredentialFilling(LoginStatusResult login_result) {
  switch (login_result) {
    case LoginStatusResult::kSuccessUsernameAndPasswordFilled:
    case LoginStatusResult::kSuccessUsernameFilled:
    case LoginStatusResult::kSuccessPasswordFilled:
      return true;
    case LoginStatusResult::kSuccessFederated:
    case LoginStatusResult::kErrorNoSigninForm:
    case LoginStatusResult::kErrorInvalidCredential:
    case LoginStatusResult::kErrorNoFillableFields:
    case LoginStatusResult::kErrorDeviceReauthRequired:
    case LoginStatusResult::kErrorDeviceReauthFailed:
    case LoginStatusResult::kErrorFederatedContinuation:
    case LoginStatusResult::kErrorFederatedAccountNotLoggedIn:
    case LoginStatusResult::kErrorFederatedAccountIsSignUp:
    case LoginStatusResult::kErrorFederatedAccountNotAvailable:
    case LoginStatusResult::kErrorFederatedIdpReturnedError:
    case LoginStatusResult::kErrorFederatedIdpNetworkError:
    case LoginStatusResult::kErrorFederatedTokenRequestAborted:
    case LoginStatusResult::kErrorFederatedFrameNotActive:
    case LoginStatusResult::kErrorFederatedExpectedAccountNotPresent:
    case LoginStatusResult::kErrorFederatedTimeout:
    case LoginStatusResult::kRequiresButtonClick:
    case LoginStatusResult::kErrorPageChangedDuringFilling:
      return false;
  }
}

}  // namespace actor_login
