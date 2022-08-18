// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace password_manager {

using metrics_util::LogPasswordSettingsReauthResult;
using metrics_util::ReauthResult;

PasswordAccessAuthenticator::PasswordAccessAuthenticator(
    ReauthCallback os_reauth_call,
    TimeoutCallback timeout_call)
    : os_reauth_call_(std::move(os_reauth_call)),
      timeout_call_(std::move(timeout_call)) {}

PasswordAccessAuthenticator::~PasswordAccessAuthenticator() = default;

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
  os_reauth_call_.Run(
      purpose,
      base::BindOnce(&PasswordAccessAuthenticator::OnUserReauthenticationResult,
                     base::Unretained(this),
                     metrics_util::TimeCallback(
                         std::move(callback),
                         "PasswordManager.Settings.AuthenticationTime")));
}

void PasswordAccessAuthenticator::OnUserReauthenticationResult(
    AuthResultCallback callback,
    bool authenticated) {
  if (authenticated) {
    auth_timer_.Start(FROM_HERE, kAuthValidityPeriod,
                      base::BindRepeating(timeout_call_));
  }
  LogPasswordSettingsReauthResult(authenticated ? ReauthResult::kSuccess
                                                : ReauthResult::kFailure);
  std::move(callback).Run(authenticated);
}

}  // namespace password_manager
