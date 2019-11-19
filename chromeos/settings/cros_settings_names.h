// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_
#define CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_

#include "base/component_export.h"

namespace chromeos {

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kCrosSettingsPrefix[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kAccountsPrefAllowGuest[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefAllowNewUser[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefShowUserNamesOnSignIn[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kAccountsPrefUsers[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefEphemeralUsersEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccounts[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyId[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyType[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyKioskAppId[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskClass[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskAction[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginId[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefSupervisedUsersEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefTransferSAMLCookies[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefLoginScreenDomainAutoComplete[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kSignedDataRoamingEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kUpdateDisabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kTargetVersionPrefix[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAllowedConnectionTypesForUpdate[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kSystemTimezonePolicy[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kSystemTimezone[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kSystemUse24HourClock[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDeviceOwner[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kStatsReportingPref[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReleaseChannel[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReleaseChannelDelegated[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceVersionInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceActivityTimes[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceBoardStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceBootMode[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceLocation[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceNetworkInterfaces[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDevicePowerStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceStorageStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceUsers[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceHardwareStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceSessionStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportOsUpdateStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportRunningKioskApp[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportUploadFrequency[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kHeartbeatEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kHeartbeatFrequency[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kSystemLogUploadEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kPolicyMissingMitigationMode[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAllowRedeemChromeOsRegistrationOffers[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kStartUpFlags[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kKioskAppSettingsPrefix[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const int kKioskAppSettingsPrefixLength;
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kKioskApps[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kKioskAutoLaunch[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kKioskDisableBailoutShortcut[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kVariationsRestrictParameter[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceAttestationEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAttestationForContentProtectionEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kServiceAccountIdentity[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDeviceDisabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDeviceDisabledMessage[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kRebootOnShutdown[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kExtensionCacheSize[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolution[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalWidth[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalHeight[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalScale[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalUseNative[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolutionKeyInternalScale[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDisplayResolutionKeyRecommended[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDisplayRotationDefault[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kLoginAuthenticationBehavior[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kAllowBluetooth[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDeviceWiFiAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceQuirksDownloadEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kLoginVideoCaptureAllowedUrls[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceLoginScreenExtensions[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceLoginScreenLocales[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceLoginScreenInputMethods[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceLoginScreenSystemInfoEnforced[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceShowNumericKeyboardForPassword[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kPerUserTimezoneEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kFineGrainedTimeZoneResolveEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDeviceOffHours[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceNativePrintersAccessMode[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceNativePrintersBlacklist[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceNativePrintersWhitelist[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kTPMFirmwareUpdateSettings[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kMinimumRequiredChromeVersion[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kCastReceiverName[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kUnaffiliatedArcAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kDeviceHostnameTemplate[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kVirtualMachinesAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kSamlLoginAuthenticationType[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceAutoUpdateTimeRestrictions[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceUnaffiliatedCrostiniAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kPluginVmAllowed[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kPluginVmLicenseKey[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceRebootOnUserSignout[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceWilcoDtcAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceDockMacAddressSource[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceScheduledUpdateCheck[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceSecondFactorAuthenticationMode[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDevicePowerwashAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceWebBasedAttestationAllowedUrls[];

}  // namespace chromeos

#endif  // CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_
