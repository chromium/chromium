// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_POLICY_METRICS_RECORDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_POLICY_METRICS_RECORDER_H_

#include "base/scoped_observation.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_export.h"

namespace policy {

constexpr char kEnterpriseUserPolicyCountHistogram[] =
    "Enterprise.UserPolicy.Count";

// Recorder of user policy metrics. Starts recording on construction. Only
// record the metrics once during its lifetime.
class POLICY_EXPORT UserPolicyMetricsRecorder
    : public ConfigurationPolicyProvider::Observer {
 public:
  UserPolicyMetricsRecorder(ConfigurationPolicyProvider* provider);
  ~UserPolicyMetricsRecorder() override;
  UserPolicyMetricsRecorder(const UserPolicyMetricsRecorder&) = delete;
  UserPolicyMetricsRecorder& operator=(const UserPolicyMetricsRecorder&) =
      delete;

 private:
  // ConfigurationPolicyProvider::Observer implementation:
  void OnUpdatePolicy(ConfigurationPolicyProvider* provider) override;

  void RecordUserPolicyCount(ConfigurationPolicyProvider* provider);
  void Stop();

  base::ScopedObservation<ConfigurationPolicyProvider,
                          ConfigurationPolicyProvider::Observer>
      scoped_policy_provider_observation_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_POLICY_METRICS_RECORDER_H_
