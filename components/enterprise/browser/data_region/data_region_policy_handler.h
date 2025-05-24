// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_DATA_REGION_DATA_REGION_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_DATA_REGION_DATA_REGION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class PolicyErrorMap;

class DataRegionPolicyHandler : public IntRangePolicyHandler {
 public:
  DataRegionPolicyHandler(const char* policy_name, const char* pref_path);
  ~DataRegionPolicyHandler() override;

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
};
}  // namespace policy

#endif  // COMPONENTS_ENTERPRISE_BROWSER_DATA_REGION_DATA_REGION_POLICY_HANDLER_H_
