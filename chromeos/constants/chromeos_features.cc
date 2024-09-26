// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::features {

// Adds Managed APN Policies support.
BASE_FEATURE(kApnPolicies, "ApnPolicies", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables smaller battery badge icons to improve legibility of the battery
// percentage.
BASE_FEATURE(kBatteryBadgeIcon,
             "BatteryBadgeIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables more filtering out of phones from the Bluetooth UI.
BASE_FEATURE(kBluetoothPhoneFilter,
             "BluetoothPhoneFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables better quick settings UI for bluetooth and wifi error states.
BASE_FEATURE(kBluetoothWifiQSPodRefresh,
             "BluetoothWifiQSPodRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables show captive portal signin in a specially flagged popup window.
BASE_FEATURE(kCaptivePortalPopupWindow,
             "CaptivePortalPopupWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables updated UI for the clipboard history menu and new system behavior
// related to clipboard history.
BASE_FEATURE(kClipboardHistoryRefresh,
             "ClipboardHistoryRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables cloud game features. A separate flag "LauncherGameSearch" controls
// launcher-only cloud gaming features, since they can also be enabled on
// non-cloud-gaming devices.
BASE_FEATURE(kCloudGamingDevice,
             "CloudGamingDevice",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables MPS to push payload to chrome devices.
BASE_FEATURE(kAlmanacLauncherPayload,
             "AlmanacLauncherPayload",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Apps APIs.
BASE_FEATURE(kBlinkExtension,
             "BlinkExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the ChromeOS Diagnostics API.
BASE_FEATURE(kBlinkExtensionDiagnostics,
             "BlinkExtensionDiagnostics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Kiosk APIs.
BASE_FEATURE(kBlinkExtensionKiosk,
             "BlinkExtensionKiosk",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag used to gate preinstallation of the container app.
BASE_FEATURE(kContainerAppPreinstall,
             "ContainerAppPreinstall",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag used to gate debugging preinstallation of the container app.
BASE_FEATURE(kContainerAppPreinstallDebug,
             "ContainerAppPreinstallDebug",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables handling of key press event in background.
BASE_FEATURE(kCrosAppsBackgroundEventHandling,
             "CrosAppsBackgroundEventHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of cros-component UI elements. Contact:
// cros-jellybean-team@google.com.
BASE_FEATURE(kCrosComponents,
             "CrosComponents",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables an app to discover and install other apps. This flag will be enabled
// with Finch.
BASE_FEATURE(kCrosMall, "CrosMall", base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the Mall app will be installed as an SWA. Only takes effect
// when CrosMall is enabled. This flag will be enabled with Finch.
BASE_FEATURE(kCrosMallSwa, "CrosMallSwa", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables denying file access to dlp protected files in MyFiles.
BASE_FEATURE(kDataControlsFileAccessDefaultDeny,
             "DataControlsFileAccessDefaultDeny",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables data migration.
BASE_FEATURE(kDataMigration,
             "DataMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the desk profiles feature.
BASE_FEATURE(kDeskProfiles, "DeskProfiles", base::FEATURE_DISABLED_BY_DEFAULT);

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the ChromeOS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
BASE_FEATURE(kDisableIdleSocketsCloseOnMemoryPressure,
             "disable_idle_sockets_close_on_memory_pressure",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
BASE_FEATURE(kDisableOfficeEditingComponentApp,
             "DisableOfficeEditingComponentApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables translation services of the Quick Answers V2.
BASE_FEATURE(kDisableQuickAnswersV2Translation,
             "DisableQuickAnswersV2Translation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables import of PKCS12 files to software backed Chaps storage together with
// import to NSS DB via the "Import" button in the certificates manager.
// When the feature is disabled, PKCS12 files are imported to NSS DB only.
BASE_FEATURE(kEnablePkcs12ToChapsDualWrite,
             "EnablePkcs12ToChapsDualWrite",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Essential Search in Omnibox for both launcher and browser.
BASE_FEATURE(kEssentialSearch,
             "EssentialSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable experimental goldfish web app isolation.
BASE_FEATURE(kExperimentalWebAppStoragePartitionIsolation,
             "ExperimentalWebAppStoragePartitionIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Jelly features. go/jelly-flags
BASE_FEATURE(kJelly, "Jelly", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Jellyroll features. Jellyroll is a feature flag for CrOSNext, which
// controls all system UI updates and new system components. go/jelly-flags
BASE_FEATURE(kJellyroll, "Jellyroll", base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables Kiosk Heartbeats to be sent via Encrypted Reporting Pipeline
BASE_FEATURE(kKioskHeartbeatsViaERP,
             "KioskHeartbeatsViaERP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi feature.
BASE_FEATURE(kMahi, "Mahi", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi feature from the feature management
// module.
BASE_FEATURE(kFeatureManagementMahi,
             "FeatureManagementMahi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether mahi sends url when making request to the server.
BASE_FEATURE(kMahiSendingUrl,
             "MahiSendingUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Mahi for managed users.
BASE_FEATURE(kMahiManaged, "MahiManaged", base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Controls enabling / disabling the sparky feature.
BASE_FEATURE(kSparky, "Sparky", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi debugging.
BASE_FEATURE(kMahiDebugging,
             "MahiDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Changes the ChromeOS notification width size from 360px to 400px for pop-up
// notifications and 344px to 400px for notifications in the message center.
BASE_FEATURE(kNotificationWidthIncrease,
             "NotificationWidthIncrease",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature.
BASE_FEATURE(kOrca, "Orca", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature for dogfood population.
BASE_FEATURE(kOrcaDogfood, "OrcaDogfood", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables Orca internationalization.
BASE_FEATURE(kOrcaInternationalize,
             "OrcaInternationalize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling orca l10n strings.
BASE_FEATURE(kOrcaUseL10nStrings,
             "OrcaUseL10nStrings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether a set of UI optimizations within `OverviewSession::Init()` are
// enabled or not. These should have no user-visible impact, except a faster
// presentation time for the first frame of most overview sessions.
BASE_FEATURE(kOverviewSessionInitOptimizations,
             "OverviewSessionInitOptimizations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature management flag used to gate preinstallation of the container app.
// This flag is meant to be enabled by the feature management module.
BASE_FEATURE(kFeatureManagementContainerAppPreinstall,
             "FeatureManagementContainerAppPreinstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the history embedding feature from the
// feature management module.
BASE_FEATURE(kFeatureManagementHistoryEmbedding,
             "FeatureManagementHistoryEmbedding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature from the feature management
// module.
BASE_FEATURE(kFeatureManagementOrca,
             "FeatureManagementOrca",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to disable chrome compose.
BASE_FEATURE(kFeatureManagementDisableChromeCompose,
             "FeatureManagementDisableChromeCompose",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables rounded windows. This flag is intended to be controlled by the
// feature management module.
BASE_FEATURE(kFeatureManagementRoundedWindows,
             "FeatureManagementRoundedWindows",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether PreinstalledWebApps should only install core apps.
BASE_FEATURE(kPreinstalledWebAppsCoreOnly,
             "PreinstalledWebAppsCoreOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable quick answers V2 settings sub-toggles.
BASE_FEATURE(kQuickAnswersV2SettingsSubToggle,
             "QuickAnswersV2SettingsSubToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Quick Answers Rich card.
BASE_FEATURE(kQuickAnswersRichCard,
             "QuickAnswersRichCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Material Next UI for Quick Answers.
BASE_FEATURE(kQuickAnswersMaterialNextUI,
             "QuickAnswersMaterialNextUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Quick Share v2, which defaults Quick Share to 'Your Devices'
// visibility, removes the 'Selected Contacts' visibility, removes the Quick
// Share On/Off toggle, and adds a visibility dialog menu to Quick Settings.
BASE_FEATURE(kQuickShareV2, "QuickShareV2", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsQuickShareV2Enabled() {
  return base::FeatureList::IsEnabled(kQuickShareV2);
}

// Enables the Office files upload workflow to improve Office files support.
BASE_FEATURE(kUploadOfficeToCloud,
             "UploadOfficeToCloud",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Office files upload workflow for enterprise users to improve
// Office files support.
BASE_FEATURE(kUploadOfficeToCloudForEnterprise,
             "UploadOfficeToCloudForEnterprise",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Microsoft OneDrive integration workflow for enterprise users to
// cloud integration support.
BASE_FEATURE(kMicrosoftOneDriveIntegrationForEnterprise,
             "MicrosoftOneDriveIntegrationForEnterprise",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRoundedWindows,
             "RoundedWindows",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables CloudFileSystem for FileSystemProvider extensions.
BASE_FEATURE(kFileSystemProviderCloudFileSystem,
             "FileSystemProviderCloudFileSystem",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a content cache in CloudFileSystem for FileSystemProvider extensions.
BASE_FEATURE(kFileSystemProviderContentCache,
             "FileSystemProviderContentCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kRoundedWindowsRadius[] = "window_radius";

bool IsApnPoliciesEnabled() {
  return base::FeatureList::IsEnabled(kApnPolicies);
}

bool IsBatteryBadgeIconEnabled() {
  return base::FeatureList::IsEnabled(kBatteryBadgeIcon);
}

bool IsBluetoothWifiQSPodRefreshEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothWifiQSPodRefresh);
}

bool IsCaptivePortalPopupWindowEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()
      ->IsCaptivePortalPopupWindowEnabled();
#else
  return base::FeatureList::IsEnabled(kCaptivePortalPopupWindow);
#endif
}

bool IsClipboardHistoryRefreshEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->EnableClipboardHistoryRefresh();
#else
  return base::FeatureList::IsEnabled(kClipboardHistoryRefresh) &&
         IsJellyEnabled();
#endif
}

bool IsCloudGamingDeviceEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsCloudGamingDevice();
#else
  return base::FeatureList::IsEnabled(kCloudGamingDevice);
#endif
}

bool IsAlmanacLauncherPayloadEnabled() {
  return base::FeatureList::IsEnabled(kAlmanacLauncherPayload);
}

bool IsBlinkExtensionEnabled() {
  return base::FeatureList::IsEnabled(kBlinkExtension);
}

bool IsBlinkExtensionDiagnosticsEnabled() {
  return IsBlinkExtensionEnabled() &&
         base::FeatureList::IsEnabled(kBlinkExtensionDiagnostics);
}

bool IsContainerAppPreinstallEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsContainerAppPreinstallEnabled();
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return (base::FeatureList::IsEnabled(
              kFeatureManagementContainerAppPreinstall) ||
          IsContainerAppPreinstallDebugEnabled()) &&
         base::FeatureList::IsEnabled(kContainerAppPreinstall);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
}

bool IsContainerAppPreinstallDebugEnabled() {
  // NOTE: Feature management takes precedence over debugging.
  if (base::FeatureList::IsEnabled(kFeatureManagementContainerAppPreinstall)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kContainerAppPreinstallDebug);
}

bool IsCrosComponentsEnabled() {
  return base::FeatureList::IsEnabled(kCrosComponents) && IsJellyEnabled();
}

bool IsCrosMallWebAppEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsCrosMallWebAppEnabled();
#else
  return base::FeatureList::IsEnabled(kCrosMall) &&
         !base::FeatureList::IsEnabled(kCrosMallSwa);
#endif
}

bool IsCrosMallSwaEnabled() {
  return base::FeatureList::IsEnabled(kCrosMall) &&
         base::FeatureList::IsEnabled(kCrosMallSwa);
}

bool IsDataControlsFileAccessDefaultDenyEnabled() {
  return base::FeatureList::IsEnabled(kDataControlsFileAccessDefaultDeny);
}

bool IsDataMigrationEnabled() {
  return base::FeatureList::IsEnabled(kDataMigration);
}

bool IsDeskProfilesEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsDeskProfilesEnabled();
#else
  return base::FeatureList::IsEnabled(kDeskProfiles);
#endif
}

bool IsEssentialSearchEnabled() {
  return base::FeatureList::IsEnabled(kEssentialSearch);
}

bool IsFileSystemProviderCloudFileSystemEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()
      ->IsFileSystemProviderCloudFileSystemEnabled();
#else
  return base::FeatureList::IsEnabled(kFileSystemProviderCloudFileSystem);
#endif
}

bool IsFileSystemProviderContentCacheEnabled() {
  // The `ContentCache` will be owned by the `CloudFileSystem`. Thus, the
  // `FileSystemProviderCloudFileSystem` flag has to be enabled too.
  if (!IsFileSystemProviderCloudFileSystemEnabled()) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()
      ->IsFileSystemProviderContentCacheEnabled();
#else
  return base::FeatureList::IsEnabled(kFileSystemProviderContentCache);
#endif
}

bool IsJellyEnabled() {
  return base::FeatureList::IsEnabled(kJelly);
}

bool IsJellyrollEnabled() {
  // Only enable Jellyroll if Jelly is also enabled as this is how tests expect
  // this to behave.
  return IsJellyEnabled() && base::FeatureList::IsEnabled(kJellyroll);
}

// Sparkly depends on Mahi, so we turn on Mahi if the sparky flag is enabled.
// Sparky doesn't work on LACROS so that case is ignored.
bool IsMahiEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsMahiEnabled();
#else
  return (base::FeatureList::IsEnabled(kMahi) &&
          base::FeatureList::IsEnabled(kFeatureManagementMahi)) ||
         base::FeatureList::IsEnabled(kSparky);
#endif
}

// Mahi requests are composed & sent from ash.
bool IsMahiSendingUrl() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(kMahiSendingUrl);
#else
  return false;
#endif
}

bool IsMahiManagedEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(kMahiManaged);
#else
  return false;
#endif
}

bool IsSparkyEnabled() {
  return base::FeatureList::IsEnabled(kSparky);
}

bool IsMahiDebuggingEnabled() {
  return base::FeatureList::IsEnabled(kMahiDebugging);
}

bool IsNotificationWidthIncreaseEnabled() {
  return base::FeatureList::IsEnabled(kNotificationWidthIncrease);
}

bool IsOrcaEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsOrcaEnabled();
#else
  return base::FeatureList::IsEnabled(chromeos::features::kOrcaDogfood) ||
         (base::FeatureList::IsEnabled(chromeos::features::kOrca) &&
          base::FeatureList::IsEnabled(kFeatureManagementOrca));
#endif
}

bool IsOrcaUseL10nStringsEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsOrcaUseL10nStringsEnabled();
#else
  return base::FeatureList::IsEnabled(chromeos::features::kOrcaUseL10nStrings);
#endif
}

bool IsOrcaInternationalizeEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsOrcaInternationalizeEnabled();
#else
  return base::FeatureList::IsEnabled(
      chromeos::features::kOrcaInternationalize);
#endif
}

bool ShouldDisableChromeComposeOnChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()
      ->ShouldDisableChromeComposeOnChromeOS();
#else
  return base::FeatureList::IsEnabled(kFeatureManagementDisableChromeCompose) ||
         IsOrcaEnabled();
#endif
}

bool IsQuickAnswersMaterialNextUIEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersMaterialNextUI);
}

bool IsQuickAnswersV2TranslationDisabled() {
  return base::FeatureList::IsEnabled(kDisableQuickAnswersV2Translation);
}

bool IsQuickAnswersRichCardEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersRichCard);
}

bool IsQuickAnswersV2SettingsSubToggleEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersV2SettingsSubToggle);
}

bool IsUploadOfficeToCloudEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsUploadOfficeToCloudEnabled();
#else
  return base::FeatureList::IsEnabled(kUploadOfficeToCloud);
#endif
}

bool IsUploadOfficeToCloudForEnterpriseEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/296282654): Implement propagation if necessary.
  return false;
#else
  return base::FeatureList::IsEnabled(kUploadOfficeToCloud) &&
         base::FeatureList::IsEnabled(kUploadOfficeToCloudForEnterprise);
#endif
}

bool IsMicrosoftOneDriveIntegrationForEnterpriseEnabled() {
  return IsUploadOfficeToCloudEnabled() &&
         base::FeatureList::IsEnabled(
             kMicrosoftOneDriveIntegrationForEnterprise);
}

bool IsRoundedWindowsEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementRoundedWindows) &&
         base::FeatureList::IsEnabled(kRoundedWindows);
}

bool IsPkcs12ToChapsDualWriteEnabled() {
  return base::FeatureList::IsEnabled(kEnablePkcs12ToChapsDualWrite);
}

bool IsFeatureManagementHistoryEmbeddingEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementHistoryEmbedding);
}

bool AreOverviewSessionInitOptimizationsEnabled() {
  return base::FeatureList::IsEnabled(kOverviewSessionInitOptimizations);
}

int RoundedWindowsRadius() {
  if (!IsRoundedWindowsEnabled()) {
    return 0;
  }

  return base::GetFieldTrialParamByFeatureAsInt(kRoundedWindows,
                                                kRoundedWindowsRadius,
                                                /*default_value=*/12);
}

}  // namespace chromeos::features
