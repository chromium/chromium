// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

namespace {
Credential::Id GenerateCredentialId() {
  static Credential::Id::Generator generator;
  return generator.GenerateNextId();
}
}  // namespace

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
  };
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
  };
}

}  // namespace actor_login
