// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CLOUD_REPORTING_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CLOUD_REPORTING_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace enterprise_reporting {

// Sets prefs for policy CloudReportingEnabled policy.
//
// Show an error message iff the machine is not enrolled with the Chrome Browser
// Cloud Management when this policy is applied.
class CloudReportingPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  CloudReportingPolicyHandler();
  CloudReportingPolicyHandler(const CloudReportingPolicyHandler&) = delete;
  CloudReportingPolicyHandler& operator=(const CloudReportingPolicyHandler&) =
      delete;
  ~CloudReportingPolicyHandler() override;

  // policy::TypeCheckingPolicyHandler
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_CLOUD_REPORTING_POLICY_HANDLER_H_
