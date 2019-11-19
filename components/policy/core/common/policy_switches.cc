// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_switches.h"

namespace policy {
namespace switches {

// Specifies the URL at which to communicate with the device management backend
// to fetch configuration policies and perform other device tasks.
const char kDeviceManagementUrl[]           = "device-management-url";

// Disables fetching and storing cloud policy for components.
const char kDisableComponentCloudPolicy[]   = "disable-component-cloud-policy";

// Specifies the URL at which to upload real-time reports.
const char kRealtimeReportingUrl[] = "realtime-reporting-url";

// Always treat user as affiliated.
// TODO(antrim): Remove once test servers correctly produce affiliation ids.
const char kUserAlwaysAffiliated[]  = "user-always-affiliated";

}  // namespace switches
}  // namespace policy
