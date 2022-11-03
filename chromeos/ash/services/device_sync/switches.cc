// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/switches.h"

namespace ash {

namespace device_sync {

namespace switches {

// Overrides the default URL for Google APIs (https://www.googleapis.com) used
// by CryptAuth.
const char kCryptAuthHTTPHost[] = "cryptauth-http-host";

// Overrides the default URL for CryptAuth v2 Enrollment:
// https://cryptauthenrollment.googleapis.com.
const char kCryptAuthV2EnrollmentHTTPHost[] =
    "cryptauth-v2-enrollment-http-host";

// Overrides the default URL for CryptAuth v2 DeviceSync:
// https://cryptauthdevicesync.googleapis.com.
const char kCryptAuthV2DeviceSyncHTTPHost[] =
    "cryptauth-v2-devicesync-http-host";

}  // namespace switches

}  // namespace device_sync

}  // namespace ash
