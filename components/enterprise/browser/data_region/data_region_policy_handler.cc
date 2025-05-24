// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/data_region/data_region_policy_handler.h"

namespace policy {

DataRegionPolicyHandler::DataRegionPolicyHandler(const char* policy_name,
                                                 const char* pref_path)
    : IntRangePolicyHandler(policy_name, pref_path, 0, 2, false) {}
DataRegionPolicyHandler::~DataRegionPolicyHandler() = default;

bool DataRegionPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                  PolicyErrorMap* errors) {
  if (!CloudOnlyPolicyHandler::CheckCloudOnlyPolicySettings(policy_name(),
                                                            policies, errors) ||
      !IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }
  return true;
}

}  // namespace policy
