// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SWITCHES_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SWITCHES_H_

namespace ash {

namespace device_sync {

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kCryptAuthHTTPHost[];
extern const char kCryptAuthV2EnrollmentHTTPHost[];
extern const char kCryptAuthV2DeviceSyncHTTPHost[];

}  // namespace switches

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SWITCHES_H_
