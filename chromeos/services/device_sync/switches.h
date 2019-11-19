// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_SWITCHES_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_SWITCHES_H_

namespace chromeos {

namespace device_sync {

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kCryptAuthHTTPHost[];
extern const char kCryptAuthV2EnrollmentHTTPHost[];
extern const char kCryptAuthV2DeviceSyncHTTPHost[];

}  // namespace switches

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_SWITCHES_H_
