// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_access_authenticator.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
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

void PasswordAccessAuthenticator::Init(TimeoutCallback timeout_call) {
  timeout_call_ = std::move(timeout_call);
}

// static
base::TimeDelta PasswordAccessAuthenticator::GetAuthValidityPeriod() {
  if (!base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    return base::Seconds(60);
  }
  return syncer::kPasswordNotesAuthValidity.Get();
}

void PasswordAccessAuthenticator::RestartAuthTimer() {
  if (timeout_timer_.IsRunning()) {
    timeout_timer_.Reset();
  }
}

void PasswordAccessAuthenticator::OnUserReauthenticationResult(
    bool authenticated) {
  if (authenticated) {
    CHECK(!timeout_call_.is_null());
    timeout_timer_.Start(FROM_HERE, GetAuthValidityPeriod(),
                         base::BindRepeating(timeout_call_));
  }
  // TODO(crbug.com/1476842): Replace this metric with one which has only 2
  // buckets.
  LogPasswordSettingsReauthResult(authenticated ? ReauthResult::kSuccess
                                                : ReauthResult::kFailure);
}

}  // namespace password_manager
