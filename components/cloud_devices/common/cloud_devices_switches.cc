// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/cloud_devices_switches.h"

namespace switches {

// The URL of the cloud print service to use, overrides any value stored in
// preferences, and the default. Only used if the cloud print service has been
// enabled. Used for testing.
const char kCloudPrintURL[] = "cloud-print-url";

}  // namespace switches
