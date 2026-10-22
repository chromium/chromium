// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_OS_SECURITY_INFO_PROVIDER_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_OS_SECURITY_INFO_PROVIDER_H_

namespace device_signals {

// Provides device security information queried synchronously from Android OS
// APIs via JNI. Methods return raw OS values; mapping them to signal values is
// left to the caller so that the mapping logic can be unit-tested.
//
// Methods are virtual only so that tests can override them.
class OsSecurityInfoProvider {
 public:
  OsSecurityInfoProvider();
  virtual ~OsSecurityInfoProvider();

  OsSecurityInfoProvider(const OsSecurityInfoProvider&) = delete;
  OsSecurityInfoProvider& operator=(const OsSecurityInfoProvider&) = delete;

  // Returns whether the device is secured with a PIN, pattern or password, as
  // reported by KeyguardManager.isDeviceSecure().
  virtual bool IsDeviceSecure();
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_OS_SECURITY_INFO_PROVIDER_H_
