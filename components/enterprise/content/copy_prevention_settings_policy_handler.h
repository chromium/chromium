// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONTENT_COPY_PREVENTION_SETTINGS_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_CONTENT_COPY_PREVENTION_SETTINGS_POLICY_HANDLER_H_

#include <string>

#include "components/policy/core/browser/configuration_policy_handler.h"

class CopyPreventionSettingsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  CopyPreventionSettingsPolicyHandler(const char* policy_name,
                                      const char* pref_path,
                                      policy::Schema schema);
  CopyPreventionSettingsPolicyHandler(CopyPreventionSettingsPolicyHandler&) =
      delete;
  CopyPreventionSettingsPolicyHandler& operator=(
      CopyPreventionSettingsPolicyHandler&) = delete;
  ~CopyPreventionSettingsPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::string pref_path_;
};

#endif  // COMPONENTS_ENTERPRISE_CONTENT_COPY_PREVENTION_SETTINGS_POLICY_HANDLER_H_
