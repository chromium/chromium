// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_policy_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"

namespace policy {

namespace {

// Counts the number of policies in `bundle`.
int CountNumberOfPoliciesOfProvider(ConfigurationPolicyProvider* provider) {
  const PolicyMap& policy_map = provider->policies().Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  return policy_map.size();
}

}  // namespace

UserPolicyMetricsRecorder::UserPolicyMetricsRecorder(
    ConfigurationPolicyProvider* provider)
    : scoped_policy_provider_observation_(this) {
  CHECK(provider);

  scoped_policy_provider_observation_.Observe(provider);
}

UserPolicyMetricsRecorder::~UserPolicyMetricsRecorder() = default;

void UserPolicyMetricsRecorder::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  RecordUserPolicyCount(provider);
}

void UserPolicyMetricsRecorder::RecordUserPolicyCount(
    ConfigurationPolicyProvider* provider) {
  UMA_HISTOGRAM_COUNTS_1000(kEnterpriseUserPolicyCountHistogram,
                            CountNumberOfPoliciesOfProvider(provider));
  Stop();
}

void UserPolicyMetricsRecorder::Stop() {
  scoped_policy_provider_observation_.Reset();
}

}  // namespace policy
