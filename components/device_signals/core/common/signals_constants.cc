// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

namespace names {

// Sub-property name for representing an Agent ID value. This is used for
// forwarding third-party agent signals.
const char kAgentId[] = "agentId";

// Name of the signal for getting information of the AllowScreenLock
// policy https://chromeenterprise.google/policies/?policy=AllowScreenLock.
const char kAllowScreenLock[] = "allowScreenLock";

// Name of the signal for getting information about the browser version.
const char kBrowserVersion[] = "browserVersion";

// Name of the signal for getting information about whether a built in
// dns is enabled on the device.
const char kBuiltInDnsClientEnabled[] = "builtInDnsClientEnabled";

// Name of the signal for getting information about whether users can
// access other computers from Chrome using Chrome Remote Desktop.
const char kChromeRemoteDesktopAppBlocked[] = "chromeRemoteDesktopAppBlocked";

// Name of a signal object containing information about a CrowdStrike agent
// currently installed.
const char kCrowdStrike[] = "crowdStrike";

// Sub-property name for representing a Customer ID value. This is used for
// forwarding third-party agent signals.
const char kCustomerId[] = "customerId";

// Customer IDs of organizations that are affiliated with the organization
// that is currently managing the device (or browser for non-CrOS platforms).
const char kDeviceAffiliationIds[] = "deviceAffiliationIds";

// Host name of the current device.
const char kDeviceHostName[] = "deviceHostName";

// Name of the signal for getting information about the device
// manufacturer (e.g. Dell).
const char kDeviceManufacturer[] = "deviceManufacturer";

// Name of the signal for getting information about the device model
// (e.g. iPhone 12 Max).
const char kDeviceModel[] = "deviceModel";

// Name of the signal for getting information about whether the device's main
// disk is encrypted.
const char kDiskEncrypted[] = "diskEncrypted";

// Name of the signal for getting information about the human readable
// name for the device.
const char kDisplayName[] = "displayName";

// Name of the signal for getting information about the CBCM enrollment
// domain of the browser or ChromeOS device.
const char kDeviceEnrollmentDomain[] = "deviceEnrollmentDomain";

// Name of the signal for getting information about whether firewall is
// enabled on the device.
const char kOsFirewall[] = "osFirewall";

// Name of the signal for getting information about the IMEI.
const char kImei[] = "imei";

// MAC addresses of the device.
const char kMacAddresses[] = "macAddresses";

// Name of the signal for getting information about the MEID.
const char kMeid[] = "meid";

// Name of the signal for getting information about the OS running
// on the device (e.g. Chrome OS).
const char kOs[] = "os";

// Name of the signal for getting information about the OS version
// of the device (e.g. macOS 10.15.7).
const char kOsVersion[] = "osVersion";

// Name of the signal for getting information about whether the device
// has a password reuse protection warning trigger.
const char kPasswordProtectionWarningTrigger[] =
    "passwordProtectionWarningTrigger";

// Customer IDs of organizations that are affiliated with the organization
// that is currently managing the user who is logged in to the current Chrome
// Profile.
const char kProfileAffiliationIds[] = "profileAffiliationIds";

// Indicates whether Enterprise-grade (i.e. custom) unsafe URL scanning is
// enabled or not. This setting may be controlled by an enterprise policy:
// https://chromeenterprise.google/policies/#EnterpriseRealTimeUrlCheckMode
const char kRealtimeUrlCheckMode[] = "realtimeUrlCheckMode";

// Name of the signal for getting information of the value of the
// SafeBrowsingProtectionLevel policy.
// https://chromeenterprise.google/policies/#SafeBrowsingProtectionLevel
const char kSafeBrowsingProtectionLevel[] = "safeBrowsingProtectionLevel";

// Name of the signal for getting information about whether access to
// the OS user is protected by a password.
const char kScreenLockSecured[] = "screenLockSecured";

// Indicates whether the device’s startup software has its Secure Boot feature
// enabled or not (trusted software).
const char kSecureBootEnabled[] = "secureBootEnabled";

// Name of the signal for getting information about the device serial
// number.
const char kSerialNumber[] = "serialNumber";

// Name of the signal for getting information of the value of the
// SitePerProcess policy.
// https://chromeenterprise.google/policies/#SitePerProcess
const char kSiteIsolationEnabled[] = "siteIsolationEnabled";

// Name of the signal for getting information about the dns address of
// the device.
const char kSystemDnsServers[] = "systemDnsServers";

// Name of the signal for getting information about whether third party
// blocking is enabled on the device.
const char kThirdPartyBlockingEnabled[] = "thirdPartyBlockingEnabled";

// Name of the signal for the trigger which generated the device signals.
const char kTrigger[] = "trigger";

// Name of the signal for getting information about the managed user's
// enrollment domain.
const char kUserEnrollmentDomain[] = "userEnrollmentDomain";

// Name of the signal for getting information about the windows domain
// the device has joined.
const char kWindowsMachineDomain[] = "windowsMachineDomain";

// Name of the signal for getting information about the windows domain
// the current OS user.
const char kWindowsUserDomain[] = "windowsUserDomain";

}  // namespace names

namespace errors {

// Returned when the user has not given explicit consent for a specific signal
// to be collected.
const char kConsentRequired[] = "CONSENT_REQUIRED";

// Returned when the user does not represent the current browser user, or is
// not managed.
const char kInvalidUser[] = "INVALID_USER";

// Returned when the user is not affiliated with the organization managing the
// browser.
const char kUnaffiliatedUser[] = "UNAFFILIATED_USER";

// Returned when the specified signal is not supported.
const char kUnsupported[] = "UNSUPPORTED";

// Returned when the signals collection code in unable to get a reference to
// the SystemSignalsService.
const char kMissingSystemService[] = "MISSING_SYSTEM_SERVICE";

// Returned when the signals aggregation response is missing a
// bundle/sub-response struct that was expected by a specific use-case.
const char kMissingBundle[] = "MISSING_BUNDLE";

// Returned when requesting the collection of a parameterized signal without
// parameters.
const char kMissingParameters[] = "MISSING_PARAMETERS";

// Returned when a signal could not be retrieved due to a parsing operation
// failed.
const char kParsingFailed[] = "PARSING_FAILED";

// Returned when a value was in a different format than expected.
const char kUnexpectedValue[] = "UNEXPECTED_VALUE";

}  // namespace errors

}  // namespace device_signals
