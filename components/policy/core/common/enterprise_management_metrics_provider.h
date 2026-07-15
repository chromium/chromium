// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_METRICS_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_METRICS_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#include "components/policy/policy_export.h"

namespace policy {

class ManagementService;
class PolicyService;

// MetricsProvider that logs the platform and active profiles' enterprise management status.
class POLICY_EXPORT EnterpriseManagementMetricsProvider
    : public metrics::MetricsProvider {
 public:
  struct ProfileState {
    raw_ptr<ManagementService> browser_management_service = nullptr;
    raw_ptr<PolicyService> policy_service = nullptr;
  };

  using GetProfileStatesCallback = base::RepeatingCallback<std::vector<ProfileState>()>;

  EnterpriseManagementMetricsProvider(
      ManagementService* platform_management_service,
      GetProfileStatesCallback get_profile_states_callback);

  EnterpriseManagementMetricsProvider(const EnterpriseManagementMetricsProvider&) = delete;
  EnterpriseManagementMetricsProvider& operator=(const EnterpriseManagementMetricsProvider&) = delete;

  ~EnterpriseManagementMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  raw_ptr<ManagementService> platform_management_service_;
  GetProfileStatesCallback get_profile_states_callback_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_METRICS_PROVIDER_H_
