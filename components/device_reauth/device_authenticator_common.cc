// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_reauth/device_authenticator_common.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/device_reauth/device_reauth_metrics_util.h"

using device_reauth::ReauthResult;

DeviceAuthenticatorProxy::DeviceAuthenticatorProxy() = default;
DeviceAuthenticatorProxy::~DeviceAuthenticatorProxy() = default;

DeviceAuthenticatorCommon::DeviceAuthenticatorCommon(
    DeviceAuthenticatorProxy* proxy,
    base::TimeDelta auth_validity_period,
    const std::string& auth_result_histogram)
    : device_authenticator_proxy_(proxy->GetWeakPtr()),
      auth_validity_period_(auth_validity_period),
      auth_result_histogram_(std::move(auth_result_histogram)) {}
DeviceAuthenticatorCommon::~DeviceAuthenticatorCommon() = default;

void DeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful(
    bool success) {
  if (!auth_result_histogram_.empty()) {
    base::UmaHistogramEnumeration(
        auth_result_histogram_,
        success ? ReauthResult::kSuccess : ReauthResult::kFailure);
  }
  if (!success) {
    return;
  }
  if (device_authenticator_proxy_) {
    device_authenticator_proxy_->UpdateLastGoodAuthTimestamp();
  }
}

bool DeviceAuthenticatorCommon::NeedsToAuthenticate() const {
  std::optional<base::TimeTicks> last_good_auth_timestamp;
  if (device_authenticator_proxy_) {
    last_good_auth_timestamp =
        device_authenticator_proxy_->GetLastGoodAuthTimestamp();
  }

  return !last_good_auth_timestamp.has_value() ||
         base::TimeTicks::Now() - last_good_auth_timestamp.value() >=
             auth_validity_period_;
}

void DeviceAuthenticatorCommon::RecordAuthResultSkipped() {
  if (!auth_result_histogram_.empty()) {
    base::UmaHistogramEnumeration(auth_result_histogram_,
                                  ReauthResult::kSkipped);
  }
}
