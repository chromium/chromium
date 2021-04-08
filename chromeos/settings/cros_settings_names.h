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
extern const char kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginId[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefTransferSAMLCookies[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefLoginScreenDomainAutoComplete[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAccountsPrefFamilyLinkAccountsAllowed[];

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
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReleaseLtsTag[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceChannelDowngradeBehavior[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceVersionInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceActivityTimes[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceBoardStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceBootMode[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceCpuInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceTimezoneInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceMemoryInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceBacklightInfo[];
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
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceGraphicsStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceCrashReportInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportOsUpdateStatus[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportRunningKioskApp[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportUploadFrequency[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceAppInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kReportDeviceBluetoothInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceFanInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceVpdInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDeviceSystemInfo[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kReportDevicePrintJobs[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kHeartbeatEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kHeartbeatFrequency[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kSystemLogUploadEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kPolicyMissingMitigationMode[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kAllowRedeemChromeOsRegistrationOffers[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kFeatureFlags[];

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
extern const char kDevicePrintersAccessMode[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDevicePrintersBlocklist[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDevicePrintersAllowlist[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kTPMFirmwareUpdateSettings[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceMinimumVersion[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceMinimumVersionAueMessage[];

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
extern const char kBorealisAllowedForDevice[];

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

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kSystemProxySettings[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kSystemProxySettingsKeyEnabled[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kSystemProxySettingsKeySystemServicesUsername[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kSystemProxySettingsKeySystemServicesPassword[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kSystemProxySettingsKeyAuthSchemes[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceCrostiniArcAdbSideloadingAllowed[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceShowLowDiskSpaceNotification[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS) extern const char kUsbDetachableAllowlist[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kUsbDetachableAllowlistKeyVid[];
COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kUsbDetachableAllowlistKeyPid[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDevicePeripheralDataAccessEnabled[];

COMPONENT_EXPORT(CHROMEOS_SETTINGS)
extern const char kDeviceAllowedBluetoothServices[];
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when migrated to ash/components/.
namespace ash {
using ::chromeos::kAccountsPrefAllowGuest;
using ::chromeos::kAccountsPrefAllowNewUser;
using ::chromeos::kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled;
using ::chromeos::kAccountsPrefDeviceLocalAccountAutoLoginDelay;
using ::chromeos::kAccountsPrefDeviceLocalAccountAutoLoginId;
using ::chromeos::kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline;
using ::chromeos::kAccountsPrefDeviceLocalAccounts;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskAction;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskClass;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyId;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyType;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle;
using ::chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl;
using ::chromeos::kAccountsPrefEphemeralUsersEnabled;
using ::chromeos::kAccountsPrefFamilyLinkAccountsAllowed;
using ::chromeos::kAccountsPrefLoginScreenDomainAutoComplete;
using ::chromeos::kAccountsPrefShowUserNamesOnSignIn;
using ::chromeos::kAccountsPrefTransferSAMLCookies;
using ::chromeos::kAccountsPrefUsers;
using ::chromeos::kAllowBluetooth;
using ::chromeos::kAllowedConnectionTypesForUpdate;
using ::chromeos::kAllowRedeemChromeOsRegistrationOffers;
using ::chromeos::kAttestationForContentProtectionEnabled;
using ::chromeos::kBorealisAllowedForDevice;
using ::chromeos::kCastReceiverName;
using ::chromeos::kCrosSettingsPrefix;
using ::chromeos::kDeviceAllowedBluetoothServices;
using ::chromeos::kDeviceAttestationEnabled;
using ::chromeos::kDeviceAutoUpdateTimeRestrictions;
using ::chromeos::kDeviceChannelDowngradeBehavior;
using ::chromeos::kDeviceCrostiniArcAdbSideloadingAllowed;
using ::chromeos::kDeviceDisabled;
using ::chromeos::kDeviceDisabledMessage;
using ::chromeos::kDeviceDisplayResolution;
using ::chromeos::kDeviceDockMacAddressSource;
using ::chromeos::kDeviceHostnameTemplate;
using ::chromeos::kDeviceLoginScreenInputMethods;
using ::chromeos::kDeviceLoginScreenLocales;
using ::chromeos::kDeviceLoginScreenSystemInfoEnforced;
using ::chromeos::kDeviceMinimumVersion;
using ::chromeos::kDeviceMinimumVersionAueMessage;
using ::chromeos::kDeviceOffHours;
using ::chromeos::kDeviceOwner;
using ::chromeos::kDevicePeripheralDataAccessEnabled;
using ::chromeos::kDevicePowerwashAllowed;
using ::chromeos::kDevicePrintersAccessMode;
using ::chromeos::kDevicePrintersAllowlist;
using ::chromeos::kDevicePrintersBlocklist;
using ::chromeos::kDeviceQuirksDownloadEnabled;
using ::chromeos::kDeviceRebootOnUserSignout;
using ::chromeos::kDeviceScheduledUpdateCheck;
using ::chromeos::kDeviceSecondFactorAuthenticationMode;
using ::chromeos::kDeviceShowLowDiskSpaceNotification;
using ::chromeos::kDeviceShowNumericKeyboardForPassword;
using ::chromeos::kDeviceUnaffiliatedCrostiniAllowed;
using ::chromeos::kDeviceWebBasedAttestationAllowedUrls;
using ::chromeos::kDeviceWiFiAllowed;
using ::chromeos::kDeviceWilcoDtcAllowed;
using ::chromeos::kDisplayRotationDefault;
using ::chromeos::kExtensionCacheSize;
using ::chromeos::kFeatureFlags;
using ::chromeos::kHeartbeatEnabled;
using ::chromeos::kHeartbeatFrequency;
using ::chromeos::kLoginAuthenticationBehavior;
using ::chromeos::kLoginVideoCaptureAllowedUrls;
using ::chromeos::kPluginVmAllowed;
using ::chromeos::kPluginVmLicenseKey;
using ::chromeos::kPolicyMissingMitigationMode;
using ::chromeos::kRebootOnShutdown;
using ::chromeos::kReleaseChannel;
using ::chromeos::kReleaseChannelDelegated;
using ::chromeos::kReleaseLtsTag;
using ::chromeos::kReportDeviceActivityTimes;
using ::chromeos::kReportDeviceAppInfo;
using ::chromeos::kReportDeviceBacklightInfo;
using ::chromeos::kReportDeviceBluetoothInfo;
using ::chromeos::kReportDeviceBoardStatus;
using ::chromeos::kReportDeviceBootMode;
using ::chromeos::kReportDeviceCpuInfo;
using ::chromeos::kReportDeviceCrashReportInfo;
using ::chromeos::kReportDeviceFanInfo;
using ::chromeos::kReportDeviceGraphicsStatus;
using ::chromeos::kReportDeviceHardwareStatus;
using ::chromeos::kReportDeviceLocation;
using ::chromeos::kReportDeviceMemoryInfo;
using ::chromeos::kReportDeviceNetworkInterfaces;
using ::chromeos::kReportDevicePowerStatus;
using ::chromeos::kReportDevicePrintJobs;
using ::chromeos::kReportDeviceSessionStatus;
using ::chromeos::kReportDeviceStorageStatus;
using ::chromeos::kReportDeviceSystemInfo;
using ::chromeos::kReportDeviceTimezoneInfo;
using ::chromeos::kReportDeviceUsers;
using ::chromeos::kReportDeviceVersionInfo;
using ::chromeos::kReportDeviceVpdInfo;
using ::chromeos::kReportOsUpdateStatus;
using ::chromeos::kReportRunningKioskApp;
using ::chromeos::kReportUploadFrequency;
using ::chromeos::kSamlLoginAuthenticationType;
using ::chromeos::kServiceAccountIdentity;
using ::chromeos::kSignedDataRoamingEnabled;
using ::chromeos::kStatsReportingPref;
using ::chromeos::kSystemLogUploadEnabled;
using ::chromeos::kSystemProxySettings;
using ::chromeos::kSystemTimezone;
using ::chromeos::kSystemTimezonePolicy;
using ::chromeos::kSystemUse24HourClock;
using ::chromeos::kTargetVersionPrefix;
using ::chromeos::kTPMFirmwareUpdateSettings;
using ::chromeos::kUnaffiliatedArcAllowed;
using ::chromeos::kUpdateDisabled;
using ::chromeos::kUsbDetachableAllowlist;
using ::chromeos::kUsbDetachableAllowlistKeyPid;
using ::chromeos::kUsbDetachableAllowlistKeyVid;
using ::chromeos::kVariationsRestrictParameter;
using ::chromeos::kVirtualMachinesAllowed;
}  // namespace ash

#endif  // CHROMEOS_SETTINGS_CROS_SETTINGS_NAMES_H_
