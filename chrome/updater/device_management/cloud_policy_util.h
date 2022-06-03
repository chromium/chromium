// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code in this file are branched from
// components/policy/core/common/cloud/cloud_policy_constants.h and
// components/policy/core/common/cloud/cloud_policy_util.h. See the
// TODO comment below.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_CLOUD_POLICY_UTIL_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_CLOUD_POLICY_UTIL_H_

#include <memory>
#include <string>

namespace enterprise_management {
class BrowserDeviceIdentifier;
}

namespace updater {

namespace policy {
// Implementations in this namespace are copied from //components/policy.
// TODO(crbug.com/1219760): Remove this namespace and reuse code from
// //components/policy.
extern const char kPolicyVerificationKeyHash[];

std::string GetPolicyVerificationKey();
std::string GetMachineName();
std::string GetOSVersion();
std::string GetOSPlatform();
std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
GetBrowserDeviceIdentifier();
}  // namespace policy

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_CLOUD_POLICY_UTIL_H_
