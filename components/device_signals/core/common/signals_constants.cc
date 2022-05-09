// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {
namespace names {

// Name of the signal for getting information of the AllowScreenLock
// policy https://chromeenterprise.google/policies/?policy=AllowScreenLock.
const char kAllowScreenLock[] = "allowSreenLock";

// Name of the signal for getting information about AV software installed on
// the device.
const char kAntiVirusInfo[] = "antiVirusInfo";

// Name of the signal for getting information about the browser version.
const char kBrowserVersion[] = "browserVersion";

// Name of the signal for getting information about whether a built in
// dns is enabled on the device.
const char kBuiltInDnsClientEnabled[] = "builtInDnsClientEnabled";

// Name of the signal for getting information about whether chrome cleanup
// is enabled on the device.
const char kChromeCleanupEnabled[] = "chromeCleanupEnabled";

// Name of the signal for getting information about the device id.
const char kDeviceId[] = "deviceId";

// Name of the signal for getting information about the device
// manufacturer (e.g. Dell).
const char kDeviceManufacturer[] = "deviceManufacturer";

// Name of the signal for getting information about the device model
// (e.g. iPhone 12 Max).
const char kDeviceModel[] = "deviceModel";

// Name of the signal for getting information about the human readable
// name for the device.
const char kDisplayName[] = "displayName";

// Name of the signal for getting information about the dns address of
// the device.
const char kDnsAddress[] = "dnsAddress";

// Name of the signal for getting information about the CBCM enrollment
// domain of the browser.
const char kEnrollmentDomain[] = "enrollmentDomain";

// Name of the parameterized signal for getting information from resources
// stored on the file system. This includes the presence/absence of
// files/folders, and also additional signals' extraction from executables.
const char kFileSystemInfo[] = "fileSystemInfo";

// Name of the signal for getting information about whether firewall is
// enabled on the device.
const char kFirewallOn[] = "firewallOn";

// Name of the signal for getting information about the IMEI.
const char kImei[] = "imei";

// Name of the signal for getting information about installed hotfixes on
// the device.
const char kInstalledHotfixes[] = "hotfixes";

// Name of the signal for getting information about whether the disk
// on the device is encrypted.
const char kIsDiskEncrypted[] = "isDiskEncrypted";

// Name of the signal for getting information about whether the device is
// jailbroken or modified.
const char kIsJailbroken[] = "isJailBroken";

// Name of the signal for getting information about whether access to
// the OS user is protected by a password.
const char kIsPasswordProtected[] = "isProtectedByPassword";

// Name of the signal for getting information about the MEID.
const char kMeid[] = "meid";

// Name of the signal for getting information about the obfuscated CBCM
// enrolled customer Id.
const char kObfuscatedCustomerId[] = "obfuscatedCustomerId";

// Name of the signal for getting information about the OS running
// on the device (e.g. Chrome OS).
const char kOs[] = "os";

// Name of the signal for getting information about the OS version
// of the device (e.g. macOS 10.15.7).
const char kOsVersion[] = "osVersion";

// Name of the signal for getting information about whether the device
// has a password reuse protection warning trigger.
const char kPasswordProtectionWarningTrigger[] =
    "passwordPotectionWarningTrigger";

// Name of the signal for getting information about whether users can
// access other computers from Chrome using Chrome Remote Desktop.
const char kRemoteDesktopAvailable[] = "remoteDesktopAvailable";

// Name of the signal for getting information of the value of the
// SafeBrowsingProtectionLevel policy.
// https://chromeenterprise.google/policies/#SafeBrowsingProtectionLevel
const char kSafeBrowsingProtectionLevel[] = "safeBrowsingProtectionLevel";

// Name of the signal for getting information about the device serial
// number.
const char kSerialNumber[] = "serialNumber";

// Name of the parameterized signal for getting information from settings
// storage (e.g. Registry, Plist) on the device.
const char kSettings[] = "settings";

// Name of the signal for getting information about the signed in profile
// name.
const char kSignedInProfileName[] = "signedInProfileName";

// Name of the signal for getting information of the value of the
// SitePerProcess policy.
// https://chromeenterprise.google/policies/#SitePerProcess
const char kSiteIsolationEnabled[] = "siteIsolationEnabled";

// Name of the signal for getting information about whether third party
// blocking is enabled on the device.
const char kThirdPartyBlockingEnabled[] = "thirdPartyBlockingEnabled";

// Name of the signal for getting information about the hash
// of the EKPub certificate of the TPM on the device, if available.
const char kTpmHash[] = "tpmHash";

// Name of the signal for getting information about the windows domain
// the device has joined.
const char kWindowsDomain[] = "windowsDomain";

}  // namespace names
}  // namespace device_signals
