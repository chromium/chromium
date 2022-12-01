// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/features.h"

namespace password_manager {

using metrics_util::LogPasswordSettingsReauthResult;
using metrics_util::ReauthResult;

PasswordAccessAuthenticator::PasswordAccessAuthenticator() = default;

PasswordAccessAuthenticator::~PasswordAccessAuthenticator() = default;

void PasswordAccessAuthenticator::Init(ReauthCallback os_reauth_call,
                                       TimeoutCallback timeout_call) {
  os_reauth_call_ = std::move(os_reauth_call);
  timeout_call_ = std::move(timeout_call);
}

// TODO(crbug.com/327331): Trigger Re-Auth after closing and opening the
// settings tab.
void PasswordAccessAuthenticator::EnsureUserIsAuthenticated(
    ReauthPurpose purpose,
    AuthResultCallback callback) {
  if (auth_timer_.IsRunning()) {
    LogPasswordSettingsReauthResult(ReauthResult::kSkipped);
    std::move(callback).Run(true);
  } else {
    ForceUserReauthentication(purpose, std::move(callback));
  }
}

void PasswordAccessAuthenticator::ForceUserReauthentication(
    ReauthPurpose purpose,
    AuthResultCallback callback) {
  DCHECK(!os_reauth_call_.is_null());
  DCHECK(!timeout_call_.is_null());
  os_reauth_call_.Run(
      purpose,
      base::BindOnce(&PasswordAccessAuthenticator::OnUserReauthenticationResult,
                     base::Unretained(this),
                     metrics_util::TimeCallback(
                         std::move(callback),
                         "PasswordManager.Settings.AuthenticationTime")));
}

void PasswordAccessAuthenticator::ExtendAuthValidity() {
  if (auth_timer_.IsRunning()) {
    auth_timer_.Reset();
  }
}

void PasswordAccessAuthenticator::OnUserReauthenticationResult(
    AuthResultCallback callback,
    bool authenticated) {
  if (authenticated) {
    DCHECK(!timeout_call_.is_null());
    auth_timer_.Start(FROM_HERE, GetAuthValidityPeriod(),
                      base::BindRepeating(timeout_call_));
  }
  LogPasswordSettingsReauthResult(authenticated ? ReauthResult::kSuccess
                                                : ReauthResult::kFailure);
  std::move(callback).Run(authenticated);
}

base::TimeDelta PasswordAccessAuthenticator::GetAuthValidityPeriod() {
  if (!base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup))
    return kAuthValidityPeriod;
  return syncer::kPasswordNotesAuthValidity.Get();
}

}  // namespace password_manager
