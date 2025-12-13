// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_H_

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// ActorLoginGetCredentialsResult in
// tools/metrics/histograms/metadata/password/enums.xml.
//
// LINT.IfChange(GetCredentialsResult)
enum class GetCredentialsResult {
  kErrorUnknown = 0,
  kSuccess = 1,
  kErrorServiceBusy = 2,
  kErrorInvalidTabInterface = 3,
  kErrorFillingNotAllowed = 4,
  kMaxValue = kErrorFillingNotAllowed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:ActorLoginGetCredentialsResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// ActorLoginAttemptLoginResult in
// tools/metrics/histograms/metadata/password/enums.xml.
//
// LINT.IfChange(AttemptLoginResult)
enum class AttemptLoginResult {
  kErrorUnknown = 0,
  kSuccessUsernameFilled = 1,
  kSuccessPasswordFilled = 2,
  kSuccessUsernameAndPasswordFilled = 3,
  kErrorNoSigninForm = 4,
  kErrorInvalidCredential = 5,
  kErrorNoFillableFields = 6,
  kErrorFillingNotAllowed = 7,
  kErrorServiceBusy = 8,
  kErrorInvalidTabInterface = 9,
  kErrorDeviceReauthRequired = 10,
  kErrorDeviceReauthFailed = 11,
  kMaxValue = kErrorDeviceReauthFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:ActorLoginAttemptLoginResult)

void RecordGetCredentialsResult(const CredentialsOrError& result_or_error);
void RecordAttemptLoginResult(const LoginStatusResultOrError& result_or_error);

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_H_
