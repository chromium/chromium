// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PLATFORM_SUPPORT_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PLATFORM_SUPPORT_H_

namespace arc {

// An abstract interface for checking if the ARCVM image should be installed
// from a DLC based on device policies.
class ArcPlatformSupport {
 public:
  ArcPlatformSupport();
  ArcPlatformSupport(const ArcPlatformSupport&) = delete;
  ArcPlatformSupport& operator=(const ArcPlatformSupport&) = delete;
  virtual ~ArcPlatformSupport();

  static ArcPlatformSupport* Get();

  // ARCVM image should be installed from DLC at run time if the device
  // is managed and the device policy DeviceFlexArcPreloadEnabled is enabled by
  // the administrator. This function will return true in that case.
  virtual bool IsDlcEnabled() const = 0;

  // Check the DLC requirement based on device state (whether it's managed)
  // and policies (whether DeviceFlexArcPreloadEnabled is enabled).
  virtual void CheckDlcRequirement() = 0;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PLATFORM_SUPPORT_H_
