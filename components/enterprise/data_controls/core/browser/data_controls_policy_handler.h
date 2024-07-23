// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyErrorMap;
}  // namespace policy

namespace data_controls {

class DataControlsPolicyHandler : public policy::CloudOnlyPolicyHandler {
 public:
  DataControlsPolicyHandler(const char* policy_name,
                            const char* pref_path,
                            policy::Schema schema);
  ~DataControlsPolicyHandler() override;

  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;

 private:
  const char* pref_path_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_POLICY_HANDLER_H_
