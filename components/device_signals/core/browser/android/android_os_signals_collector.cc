// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/android/android_os_signals_collector.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"

namespace device_signals {

AndroidOsSignalsCollector::AndroidOsSignalsCollector(
    policy::CloudPolicyManager* device_cloud_policy_manager)
    : BaseSignalsCollector({
          {SignalName::kOsSignals,
           base::BindRepeating(&AndroidOsSignalsCollector::GetOsSignals,
                               base::Unretained(this))},
      }),
      device_cloud_policy_manager_(device_cloud_policy_manager) {}

AndroidOsSignalsCollector::~AndroidOsSignalsCollector() = default;

void AndroidOsSignalsCollector::GetOsSignals(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  if (permission != UserPermission::kGranted &&
      permission != UserPermission::kMissingConsent) {
    std::move(done_closure).Run();
    return;
  }

  auto signal_response = std::make_unique<OsSignalsResponse>();

  response.os_signals_response = std::move(*signal_response);

  std::move(done_closure).Run();
}

}  // namespace device_signals
