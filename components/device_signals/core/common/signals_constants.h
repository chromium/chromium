// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_CONSTANTS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_CONSTANTS_H_

namespace device_signals {

// Signal names can be used as keys to store/retrieve signal values from
// dictionaries.
namespace names {

extern const char kAllowScreenLock[];
extern const char kAntiVirusInfo[];
extern const char kBrowserVersion[];
extern const char kBuiltInDnsClientEnabled[];
extern const char kChromeCleanupEnabled[];
extern const char kDeviceId[];
extern const char kDeviceManufacturer[];
extern const char kDeviceModel[];
extern const char kDisplayName[];
extern const char kDnsAddress[];
extern const char kEnrollmentDomain[];
extern const char kFileSystemInfo[];
extern const char kFirewallOn[];
extern const char kImei[];
extern const char kInstalledHotfixes[];
extern const char kIsDiskEncrypted[];
extern const char kIsJailbroken[];
extern const char kIsPasswordProtected[];
extern const char kMeid[];
extern const char kObfuscatedCustomerId[];
extern const char kOs[];
extern const char kOsVersion[];
extern const char kPasswordProtectionWarningTrigger[];
extern const char kRemoteDesktopAvailable[];
extern const char kSafeBrowsingProtectionLevel[];
extern const char kSerialNumber[];
extern const char kSettings[];
extern const char kSignedInProfileName[];
extern const char kSiteIsolationEnabled[];
extern const char kThirdPartyBlockingEnabled[];
extern const char kTpmHash[];
extern const char kWindowsDomain[];

}  // namespace names

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_CONSTANTS_H_
