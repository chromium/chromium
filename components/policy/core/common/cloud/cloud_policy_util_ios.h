// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_UTIL_IOS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_UTIL_IOS_H_

#include <string>

#include "components/policy/policy_export.h"

namespace policy {

// Returns the device name configured by the MDM administrator in the Managed
// App Configuration (`com.apple.configuration.managed`), read from the entry
// `kPolicyLoaderIOSDeviceNameKey`. Returns an empty string if it is unset,
// empty, or not a string. This doesn't check `kIOSReportMachineName`.
POLICY_EXPORT std::string GetIOSManagedDeviceName();

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_UTIL_IOS_H_
