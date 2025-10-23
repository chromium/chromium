// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace actor_login {

namespace {

void RecordGetCredentialsResult(GetCredentialsResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.ActorLogin.GetCredentials.Result", result);
}

void RecordAttemptLoginResult(AttemptLoginResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.ActorLogin.AttemptLogin.Result", result);
}

}  // namespace

void RecordGetCredentialsResult(const CredentialsOrError& result_or_error) {
  if (result_or_error.has_value()) {
    RecordGetCredentialsResult(GetCredentialsResult::kSuccess);
    return;
  }

  switch (result_or_error.error()) {
    case ActorLoginError::kServiceBusy:
      RecordGetCredentialsResult(GetCredentialsResult::kErrorServiceBusy);
      break;
    case ActorLoginError::kInvalidTabInterface:
      RecordGetCredentialsResult(
          GetCredentialsResult::kErrorInvalidTabInterface);
      break;
    case ActorLoginError::kFillingNotAllowed:
      RecordGetCredentialsResult(GetCredentialsResult::kErrorFillingNotAllowed);
      break;
    case ActorLoginError::kUnknown:
      RecordGetCredentialsResult(GetCredentialsResult::kErrorUnknown);
      break;
  }
}

void RecordAttemptLoginResult(const LoginStatusResultOrError& result_or_error) {
  if (result_or_error.has_value()) {
    switch (result_or_error.value()) {
      case LoginStatusResult::kSuccessUsernameFilled:
        RecordAttemptLoginResult(AttemptLoginResult::kSuccessUsernameFilled);
        break;
      case LoginStatusResult::kSuccessPasswordFilled:
        RecordAttemptLoginResult(AttemptLoginResult::kSuccessPasswordFilled);
        break;
      case LoginStatusResult::kSuccessUsernameAndPasswordFilled:
        RecordAttemptLoginResult(
            AttemptLoginResult::kSuccessUsernameAndPasswordFilled);
        break;
      case LoginStatusResult::kErrorNoSigninForm:
        RecordAttemptLoginResult(AttemptLoginResult::kErrorNoSigninForm);
        break;
      case LoginStatusResult::kErrorInvalidCredential:
        RecordAttemptLoginResult(AttemptLoginResult::kErrorInvalidCredential);
        break;
      case LoginStatusResult::kErrorNoFillableFields:
        RecordAttemptLoginResult(AttemptLoginResult::kErrorNoFillableFields);
        break;
      case LoginStatusResult::kErrorDeviceReauthRequired:
        RecordAttemptLoginResult(
            AttemptLoginResult::kErrorDeviceReauthRequired);
        break;
      case LoginStatusResult::kErrorDeviceReauthFailed:
        RecordAttemptLoginResult(AttemptLoginResult::kErrorDeviceReauthFailed);
        break;
    }
    return;
  }

  switch (result_or_error.error()) {
    case ActorLoginError::kServiceBusy:
      RecordAttemptLoginResult(AttemptLoginResult::kErrorServiceBusy);
      break;
    case ActorLoginError::kInvalidTabInterface:
      RecordAttemptLoginResult(AttemptLoginResult::kErrorInvalidTabInterface);
      break;
    case ActorLoginError::kFillingNotAllowed:
      RecordAttemptLoginResult(AttemptLoginResult::kErrorFillingNotAllowed);
      break;
    case ActorLoginError::kUnknown:
      RecordAttemptLoginResult(AttemptLoginResult::kErrorUnknown);
      break;
  }
}

}  // namespace actor_login
