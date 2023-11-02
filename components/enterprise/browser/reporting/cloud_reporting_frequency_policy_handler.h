// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CLOUD_REPORTING_FREQUENCY_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CLOUD_REPORTING_FREQUENCY_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace enterprise_reporting {

class CloudReportingFrequencyPolicyHandler
    : public policy::IntRangePolicyHandlerBase {
 public:
  CloudReportingFrequencyPolicyHandler();
  CloudReportingFrequencyPolicyHandler(
      const CloudReportingFrequencyPolicyHandler&) = delete;
  CloudReportingFrequencyPolicyHandler& operator=(
      const CloudReportingFrequencyPolicyHandler&) = delete;
  ~CloudReportingFrequencyPolicyHandler() override;

 private:
  // policy::IntRangePolicyHandlerBase
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CLOUD_REPORTING_FREQUENCY_POLICY_HANDLER_H_
