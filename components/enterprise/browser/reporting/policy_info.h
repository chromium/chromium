// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_POLICY_INFO_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_POLICY_INFO_H_

#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class Value;
}

namespace policy {
class CloudPolicyManager;
}

// Unit tests are in chrome\browser\enterprise\reporting\policy_info_unittest.cc
// TODO(crbug.com/40700771): Move the tests to this directory.
namespace enterprise_reporting {

void AppendChromePolicyInfoIntoProfileReport(
    const base::Value::Dict& policies,
    enterprise_management::ChromeUserProfileInfo* profile_info);

void AppendExtensionPolicyInfoIntoProfileReport(
    const base::Value::Dict& policies,
    enterprise_management::ChromeUserProfileInfo* profile_info);

void AppendCloudPolicyFetchTimestamp(
    enterprise_management::ChromeUserProfileInfo* profile_info,
    policy::CloudPolicyManager* manager);

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_POLICY_INFO_H_
