// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "components/policy/core/common/enterprise_management_status_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

EnterpriseManagementMetricsProvider::EnterpriseManagementMetricsProvider(
    ManagementService* platform_management_service,
    GetProfileStatesCallback get_profile_states_callback)
    : platform_management_service_(platform_management_service),
      get_profile_states_callback_(std::move(get_profile_states_callback)) {}

EnterpriseManagementMetricsProvider::~EnterpriseManagementMetricsProvider() = default;

void EnterpriseManagementMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // 1. Record Platform metrics.
  for (PlatformManagementStatus status :
       GetPlatformManagementStatuses(platform_management_service_)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus", status);
  }

  // 2. Record Browser/Profile metrics.
  std::vector<ProfileState> states = get_profile_states_callback_.Run();
  for (const auto& state : states) {
    for (BrowserManagementStatus status : GetBrowserManagementStatuses(
             state.browser_management_service, state.policy_service)) {
      base::UmaHistogramEnumeration(
          "Enterprise.ManagementService.BrowserManagementStatus", status);
    }
  }
}

}  // namespace policy
