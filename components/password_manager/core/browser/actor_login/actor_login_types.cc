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
  }
  NOTREACHED();
}

}  // namespace actor_login
