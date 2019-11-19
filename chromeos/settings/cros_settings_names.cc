// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/settings/cros_settings_names.h"

namespace chromeos {

const char kCrosSettingsPrefix[] = "cros.";

// All cros.accounts.* settings are stored in SignedSettings.
const char kAccountsPrefAllowGuest[] = "cros.accounts.allowBWSI";
const char kAccountsPrefAllowNewUser[] = "cros.accounts.allowGuest";
const char kAccountsPrefShowUserNamesOnSignIn[] =
    "cros.accounts.showUserNamesOnSignIn";
const char kAccountsPrefUsers[] = "cros.accounts.users";
const char kAccountsPrefEphemeralUsersEnabled[] =
    "cros.accounts.ephemeralUsersEnabled";
const char kAccountsPrefDeviceLocalAccounts[] =
    "cros.accounts.deviceLocalAccounts";
const char kAccountsPrefDeviceLocalAccountsKeyId[] = "id";
const char kAccountsPrefDeviceLocalAccountsKeyType[] = "type";
const char kAccountsPrefDeviceLocalAccountsKeyKioskAppId[] = "kiosk_app_id";
const char kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL[] =
    "kiosk_app_update_url";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage[] =
    "arc_kiosk_package";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskClass[] =
    "arc_kiosk_class";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskAction[] =
    "arc_kiosk_action";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName[] =
    "arc_kiosk_display_name";
const char kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl[] = "web_kiosk_url";
const char kAccountsPrefDeviceLocalAccountAutoLoginId[] =
    "cros.accounts.deviceLocalAccountAutoLoginId";
const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[] =
    "cros.accounts.deviceLocalAccountAutoLoginDelay";
const char kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled[] =
    "cros.accounts.deviceLocalAccountAutoLoginBailoutEnabled";
const char kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline[] =
    "cros.accounts.deviceLocalAccountPromptForNetworkWhenOffline";
const char kAccountsPrefSupervisedUsersEnabled[] =
    "cros.accounts.supervisedUsersEnabled";
const char kAccountsPrefTransferSAMLCookies[] =
    "cros.accounts.transferSAMLCookies";

// A string pref that specifies a domain name for the autocomplete option during
// user sign-in flow.
const char kAccountsPrefLoginScreenDomainAutoComplete[] =
    "cros.accounts.login_screen_domain_auto_complete";

// All cros.signed.* settings are stored in SignedSettings.
const char kSignedDataRoamingEnabled[] = "cros.signed.data_roaming_enabled";

// True if auto-update was disabled by the system administrator.
const char kUpdateDisabled[] = "cros.system.updateDisabled";

// True if a target version prefix is set by the system administrator.
const char kTargetVersionPrefix[] = "cros.system.targetVersionPrefix";

// A list of strings which specifies allowed connection types for
// update.
const char kAllowedConnectionTypesForUpdate[] =
    "cros.system.allowedConnectionTypesForUpdate";

// The first constant refers to the user setting editable in the UI. The second
// refers to the timezone policy. This seperation is necessary to allow the user
// to temporarily change the timezone for the current session and reset it to
// the policy's value on logout.
const char kSystemTimezone[] = "cros.system.timezone";
const char kSystemTimezonePolicy[] = "cros.system.timezone_policy";

// Value of kUse24HourClock user preference of device' owner.
// ChromeOS device uses this setting on login screen.
const char kSystemUse24HourClock[] = "cros.system.use_24hour_clock";

const char kDeviceOwner[] = "cros.device.owner";

const char kStatsReportingPref[] = "cros.metrics.reportingEnabled";

const char kReleaseChannel[] = "cros.system.releaseChannel";
const char kReleaseChannelDelegated[] = "cros.system.releaseChannelDelegated";

// A boolean pref that indicates whether OS & firmware version info should be
// reported along with device policy requests.
const char kReportDeviceVersionInfo[] =
    "cros.device_status.report_version_info";

// A boolean pref that indicates whether device activity times should be
// recorded and reported along with device policy requests.
const char kReportDeviceActivityTimes[] =
    "cros.device_status.report_activity_times";

// A boolean pref that determines whether the board status should be
// included in status reports to the device management server.
const char kReportDeviceBoardStatus[] =
    "cros.device_status.report_board_status";

// A boolean pref that indicates whether the state of the dev mode switch at
// boot should be reported along with device policy requests.
const char kReportDeviceBootMode[] = "cros.device_status.report_boot_mode";

// A boolean pref that indicates whether the current location should be reported
// along with device policy requests.
const char kReportDeviceLocation[] = "cros.device_status.report_location";

// Determines whether the device reports network interface types and addresses
// in device status reports to the device management server.
const char kReportDeviceNetworkInterfaces[] =
    "cros.device_status.report_network_interfaces";

// A boolean pref that determines whether the device power status should be
// included in status reports to the device management server.
const char kReportDevicePowerStatus[] =
    "cros.device_status.report_power_status";

// A boolean pref that determines whether the storage status should be
// included in status reports to the device management server.
const char kReportDeviceStorageStatus[] =
    "cros.device_status.report_storage_status";

// Determines whether the device reports recently logged in users in device
// status reports to the device management server.
const char kReportDeviceUsers[] = "cros.device_status.report_users";

// Determines whether the device reports hardware status (CPU utilization,
// disk space, etc) in device status reports to the device management server.
const char kReportDeviceHardwareStatus[] =
    "cros.device_status.report_hardware_status";

// Determines whether the device reports kiosk session status (app IDs,
// versions, etc) in device status reports to the device management server.
const char kReportDeviceSessionStatus[] =
    "cros.device_status.report_session_status";

// Determines whether the device reports os update status (update status,
// new platform version and new required platform version of the auto
// launched kiosk app).
const char kReportOsUpdateStatus[] =
    "cros.device_status.report_os_update_status";

// Determines whether the device reports the current running kiosk app (
// its app ID, version and required platform version).
const char kReportRunningKioskApp[] =
    "cros.device_status.report_running_kiosk_app";

// How frequently device status reports are uploaded, in milliseconds.
const char kReportUploadFrequency[] =
    "cros.device_status.report_upload_frequency";

// Determines whether heartbeats should be sent to the policy service via
// the GCM channel.
const char kHeartbeatEnabled[] = "cros.device_status.heartbeat_enabled";

// How frequently heartbeats are sent up, in milliseconds.
const char kHeartbeatFrequency[] = "cros.device_status.heartbeat_frequency";

// Determines whether system logs should be sent to the management server.
const char kSystemLogUploadEnabled[] =
    "cros.device_status.system_log_upload_enabled";

// This policy should not appear in the protobuf ever but is used internally to
// signal that we are running in a "safe-mode" for policy recovery.
const char kPolicyMissingMitigationMode[] =
    "cros.internal.policy_mitigation_mode";

// A boolean pref that indicates whether users are allowed to redeem offers
// through Chrome OS Registration.
const char kAllowRedeemChromeOsRegistrationOffers[] =
    "cros.echo.allow_redeem_chrome_os_registration_offers";

// A list pref storing the flags that need to be applied to the browser upon
// start-up.
const char kStartUpFlags[] = "cros.startup_flags";

// A string pref for the restrict parameter to be appended to the Variations URL
// when pinging the Variations server.
const char kVariationsRestrictParameter[] =
    "cros.variations_restrict_parameter";

// A boolean pref that indicates whether enterprise attestation is enabled for
// the device.
const char kDeviceAttestationEnabled[] = "cros.device.attestation_enabled";

// A boolean pref that indicates whether attestation for content protection is
// enabled for the device.
const char kAttestationForContentProtectionEnabled[] =
    "cros.device.attestation_for_content_protection_enabled";

// The service account identity for device-level service accounts on
// enterprise-enrolled devices.
const char kServiceAccountIdentity[] = "cros.service_account_identity";

// A boolean pref that indicates whether the device has been disabled by its
// owner. If so, the device will show a warning screen and will not allow any
// sessions to be started.
const char kDeviceDisabled[] = "cros.device_disabled";

// A string pref containing the message that should be shown to the user when
// the device is disabled.
const char kDeviceDisabledMessage[] = "cros.disabled_state.message";

// A boolean pref that indicates whether the device automatically reboots when
// the user initiates a shutdown via an UI element.  If set to true, all
// shutdown buttons in the UI will be replaced by reboot buttons.
const char kRebootOnShutdown[] = "cros.device.reboot_on_shutdown";

// An integer pref that specifies the limit of the device's extension cache
// size in bytes.
const char kExtensionCacheSize[] = "cros.device.extension_cache_size";

// A dictionary pref that sets the display resolution.
// Pref format:
// {
//   "external_width": int,
//   "external_height": int,
//   "external_use_native": bool,
//   "external_scale_percentage": int,
//   "internal_scale_percentage": int,
//   "recommended": bool
// }
const char kDeviceDisplayResolution[] = "cros.device_display_resolution";
const char kDeviceDisplayResolutionKeyExternalWidth[] = "external_width";
const char kDeviceDisplayResolutionKeyExternalHeight[] = "external_height";
const char kDeviceDisplayResolutionKeyExternalScale[] =
    "external_scale_percentage";
const char kDeviceDisplayResolutionKeyExternalUseNative[] =
    "external_use_native";
const char kDeviceDisplayResolutionKeyInternalScale[] =
    "internal_scale_percentage";
const char kDeviceDisplayResolutionKeyRecommended[] = "recommended";

// An integer pref that sets the display rotation at startup to a certain
// value, overriding the user value:
// 0 = 0 degrees rotation
// 1 = 90 degrees clockwise rotation
// 2 = 180 degrees rotation
// 3 = 270 degrees clockwise rotation
const char kDisplayRotationDefault[] = "cros.display_rotation_default";

// An integer pref that sets the behavior of the login authentication flow.
// 0 = authentication using the default GAIA flow.
// 1 = authentication using an interstitial screen that offers the user to go
// ahead via the SAML IdP of the device's enrollment domain, or go back to the
// normal GAIA login flow.
const char kLoginAuthenticationBehavior[] =
    "cros.device.login_authentication_behavior";

// A boolean pref that indicates whether bluetooth should be allowed on the
// device.
const char kAllowBluetooth[] = "cros.device.allow_bluetooth";

// A boolean pref that indicates whether WiFi should be allowed on the
// device.
const char kDeviceWiFiAllowed[] = "cros.device.wifi_allowed";

// A boolean pref to enable any pings or requests to the Quirks Server.
const char kDeviceQuirksDownloadEnabled[] =
    "cros.device.quirks_download_enabled";

// A list pref storing the security origins allowed to access the webcam
// during SAML logins.
const char kLoginVideoCaptureAllowedUrls[] =
    "cros.device.login_video_capture_allowed_urls";

// A list pref storing the apps or extensions to install on the login page. It
// is a list of strings, each string contains an extension ID and an update URL,
// delimited by a semicolon. This preference is set by an admin policy.
const char kDeviceLoginScreenExtensions[] =
    "cros.device.login_screen_extensions";

// A list pref specifying the locales allowed on the login screen. Currently
// only the first value is used, as the single locale allowed on the login
// screen.
const char kDeviceLoginScreenLocales[] = "cros.device_login_screen_locales";

// A list pref containing the input method IDs allowed on the login screen.
const char kDeviceLoginScreenInputMethods[] =
    "cros.device_login_screen_input_methods";

// A boolean pref that indicates whether the system information is forcedly
// shown (or hidden) on the login screen.
const char kDeviceLoginScreenSystemInfoEnforced[] =
    "cros.device_login_screen_system_info_enforced";

// A boolean pref that indicates whether to show numeric keyboard for entering
// password or not.
const char kDeviceShowNumericKeyboardForPassword[] =
    "cros.device_show_numeric_keyboard_for_password";

// A boolean pref that matches enable-per-user-time-zone chrome://flags value.
const char kPerUserTimezoneEnabled[] = "cros.flags.per_user_timezone_enabled";

// A boolean pref that matches enable-fine-graned-time-zone-detection
// chrome://flags value.
const char kFineGrainedTimeZoneResolveEnabled[] =
    "cros.flags.fine_grained_time_zone_detection_enabled";

// A dictionary pref containing time intervals and ignored policies.
// It's used to allow less restricted usage of Chrome OS during off-hours.
// This pref is set by an admin policy.
// Pref format:
// { "timezone" : string,
//   "intervals" : list of Intervals,
//   "ignored_policies" : string list }
// Interval dictionary format:
// { "start" : WeeklyTime,
//   "end" : WeeklyTime }
// WeeklyTime dictionary format:
// { "weekday" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday, etc.)
//   "time" : int # in milliseconds from the beginning of the day.
// }
const char kDeviceOffHours[] = "cros.device_off_hours";

// An enum specifying the access policy device printers should observe.
const char kDeviceNativePrintersAccessMode[] =
    "cros.device.native_printers_access_mode";
// A list of strings representing device printer ids for which access is
// restricted.
const char kDeviceNativePrintersBlacklist[] =
    "cros.device.native_printers_blacklist";
// A list of strings representing the list of device printer ids which are
// accessible.
const char kDeviceNativePrintersWhitelist[] =
    "cros.device.native_printers_whitelist";

// A dictionary containing parameters controlling the TPM firmware update
// functionality.
const char kTPMFirmwareUpdateSettings[] = "cros.tpm_firmware_update_settings";

// String indicating what is the minimum version of Chrome required to
// allow user sign in. If the string is empty or blank no restrictions will
// be applied.
const char kMinimumRequiredChromeVersion[] = "cros.min_version.chrome";

// String indicating what name should be advertised for casting to.
// If the string is empty or blank the system name will be used.
const char kCastReceiverName[] = "cros.device.cast_receiver.name";

// A boolean pref that indicates whether unaffiliated users are allowed to
// use ARC.
const char kUnaffiliatedArcAllowed[] = "cros.device.unaffiliated_arc_allowed";

// String that is used as a template for generating device hostname (that is
// used in DHCP requests).
// If the string contains either ASSET_ID, SERIAL_NUM or MAC_ADDR values,
// they will be substituted for real values.
// If the string is empty or blank, or the resulting hostname is not valid
// as per RFC 1035, then no hostname will be used.
const char kDeviceHostnameTemplate[] = "cros.network.hostname_template";

// A boolean pref that indicates whether running virtual machines on Chrome OS
// is allowed.
const char kVirtualMachinesAllowed[] = "cros.device.virtual_machines_allowed";

// An enum specifying the authentication type for newly added users which log in
// via SAML. See the SamlLoginAuthenticationTypeProto proto message for the list
// of possible values.
const char kSamlLoginAuthenticationType[] =
    "cros.device.saml_login_authentication_type";

// A list of time intervals during which the admin has disallowed automatic
// update checks.
const char kDeviceAutoUpdateTimeRestrictions[] =
    "cros.system.autoUpdateTimeRestrictions";

// A boolean pref that indicates whether running Crostini on Chrome OS is
// allowed for unaffiliated user.
const char kDeviceUnaffiliatedCrostiniAllowed[] =
    "cros.device.unaffiliated_crostini_allowed";

// A boolean pref that indicates whether PluginVm is allowed to run on this
// device.
const char kPluginVmAllowed[] = "cros.device.plugin_vm_allowed";
// A string pref that specifies PluginVm license key for this device.
const char kPluginVmLicenseKey[] = "cros.device.plugin_vm_license_key";

// An enum pref specifying the case when device needs to reboot on user sign
// out.
const char kDeviceRebootOnUserSignout[] = "cros.device.reboot_on_user_signout";

// A boolean pref that indicates whether running wilco diagnostics and telemetry
// controller on Chrome OS is allowed.
const char kDeviceWilcoDtcAllowed[] = "cros.device.wilco_dtc_allowed";

// An enum pref that specifies the device dock MAC address source.
const char kDeviceDockMacAddressSource[] =
    "cros.device.device_dock_mac_address_source";

// A dictionary pref that mandates the recurring schedule for update checks. The
// schedule is followed even if the device is suspended, however, it's not
// respected when the device is shutdown.
const char kDeviceScheduledUpdateCheck[] =
    "cros.device.device_scheduled_update_check";

// An enum pref that configures the operation mode of the built-in 2nd factor
// authenticator.
const char kDeviceSecondFactorAuthenticationMode[] =
    "cros.device.device_second_factor_authentication_mode";

// A boolean pref specifying if the device is allowed to powerwash.
const char kDevicePowerwashAllowed[] = "cros.device.device_powerwash_allowed";

// A list pref storing URL patterns that are allowed for device attestation
// during SAML authentication.
extern const char kDeviceWebBasedAttestationAllowedUrls[] =
    "cros.device.device_web_based_attestation_allowed_urls";
}  // namespace chromeos
