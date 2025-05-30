// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos_features.h"

namespace chromeos::features {

// Adds Managed APN Policies support.
BASE_FEATURE(kApnPolicies, "ApnPolicies", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables smaller battery badge icons to improve legibility of the battery
// percentage.
BASE_FEATURE(kBatteryBadgeIcon,
             "BatteryBadgeIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables better quick settings UI for bluetooth and wifi error states.
BASE_FEATURE(kBluetoothWifiQSPodRefresh,
             "BluetoothWifiQSPodRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables cloud game features.
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

// Enables ChromeOS Kiosk APIs.
BASE_FEATURE(kBlinkExtensionKiosk,
             "BlinkExtensionKiosk",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kCrosMall, "CrosMall", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the Mall app for managed users. Only has an effect when kCrosMall is
// also enabled.
BASE_FEATURE(kCrosMallManaged,
             "CrosMallManaged",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables denying file access to dlp protected files in MyFiles.
BASE_FEATURE(kDataControlsFileAccessDefaultDeny,
             "DataControlsFileAccessDefaultDeny",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables data migration.
BASE_FEATURE(kDataMigration,
             "DataMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables blur on various system surfaces.
BASE_FEATURE(kDisableSystemBlur,
             "DisableSystemBlur",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the ChromeOS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
BASE_FEATURE(kDisableIdleSocketsCloseOnMemoryPressure,
             "disable_idle_sockets_close_on_memory_pressure",
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

// Feature flag used to enable external display event telemetry.
BASE_FEATURE(kExternalDisplayEventTelemetry,
             "ExternalDisplayEventTelemetry",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag used to gate preinstallation of the Gemini app.
BASE_FEATURE(kGeminiAppPreinstall,
             "GeminiAppPreinstall",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Kiosk Heartbeats to be sent via Encrypted Reporting Pipeline
BASE_FEATURE(kKioskHeartbeatsViaERP,
             "KioskHeartbeatsViaERP",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new Magic Boost Consent Flow.
BASE_FEATURE(kMagicBoostRevamp,
             "MagicBoostRevamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new Magic Boost Consent Flow For Quick Answers.
BASE_FEATURE(kMagicBoostRevampForQuickAnswers,
             "MagicBoostRevampForQuickAnswers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi feature.
BASE_FEATURE(kMahi, "Mahi", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi feature from the feature management
// module.
BASE_FEATURE(kFeatureManagementMahi,
             "FeatureManagementMahi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the Mahi resize feature
// Does nothing if "Mahi" and "FeatureManagementMahi" are disabled.
BASE_FEATURE(kMahiPanelResizable,
             "MahiPanelResizable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether mahi sends url when making request to the server.
BASE_FEATURE(kMahiSendingUrl,
             "MahiSendingUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Mahi for managed users.
BASE_FEATURE(kMahiManaged, "MahiManaged", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi debugging.
BASE_FEATURE(kMahiDebugging,
             "MahiDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the pompano feature.
BASE_FEATURE(kPompano, "Pompano", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the summary of selected text feature.
BASE_FEATURE(kMahiSummarizeSelected,
             "MahiSummarizeSelected",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether NotebookLM is preinstalled.
BASE_FEATURE(kNotebookLmAppPreinstall,
             "NotebookLmAppPreinstall",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch to disable the new guest profile implementation on CrOS that is
// consistent with desktop chrome.
// TODO(crbug.com/40233408): Remove if the change is fully launched.
BASE_FEATURE(kNewGuestProfile,
             "NewGuestProfile",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes the ChromeOS notification width size from 360px to 400px for pop-up
// notifications and 344px to 400px for notifications in the message center.
BASE_FEATURE(kNotificationWidthIncrease,
             "NotificationWidthIncrease",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the Navigation Capturing Reimpl for the Office
// PWA.
BASE_FEATURE(kOfficeNavigationCapturingReimpl,
             "OfficeNavigationCapturingReimpl",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Feature management flag used to gate preinstallation of the Gemini app. This
// flag is meant to be enabled by the feature management module.
BASE_FEATURE(kFeatureManagementGeminiAppPreinstall,
             "FeatureManagementGeminiAppPreinstall",
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

// Enables the first wave of new features for the chrome.enterprise.platformKeys
// API. That includes:
//   - a new key type (RSA-OAEP) with a new allowed key usage (unwrapKey).
//   - a new API method to `setKeyTag()`, used to mark keys for future lookup.
// Other features might be added in this first wave, or be hold for the second
// wave. For additional details, see crbug.com/288880151.
BASE_FEATURE(kPlatformKeysChangesWave1,
             "PlatformKeysChangesWave1",
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

// Enables syncing of user's Office files upload workflow preferences for
// enterprise users, such as whether to ask before moving files to the cloud.
BASE_FEATURE(kUploadOfficeToCloudSync,
             "UploadOfficeToCloudSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the use of scope extensions for the Microsoft 365 PWA from finch as
// a fallback.
BASE_FEATURE(kMicrosoft365ScopeExtensions,
             "Microsoft365ScopeExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Comma separated list of scope extension URLs for the Microsoft 365 PWA.
const base::FeatureParam<std::string> kMicrosoft365ScopeExtensionsURLs{
    &kMicrosoft365ScopeExtensions, "m365-scope-extensions-urls",
    /*default*/

    // The Office editors (Word, Excel, PowerPoint) are located on the
    // OneDrive origin.
    "https://onedrive.live.com/,"

    // Links to opening Office editors go via this URL shortener origin.
    "https://1drv.ms/,"

    // The old branding of the Microsoft 365 web app. Many links within
    // Microsoft 365 still link to the old www.office.com origin.
    "https://www.office.com/,"

    // The new branding for the Microsoft 365 web app.
    "https://m365.cloud.microsoft/,"

    // The current Microsoft 365 web app. The scope of the new Microsoft 365
    // Copilot web app remains unclear, so this is added for safety.
    "https://www.microsoft365.com/"};

// Comma separated list of scope extension domains for the Microsoft 365 PWA.
const base::FeatureParam<std::string> kMicrosoft365ScopeExtensionsDomains{
    &kMicrosoft365ScopeExtensions, "m365-scope-extensions-domains",
    /*default*/

    // The OneDrive Business domain (for the extension to match
    // https://<customer>-my.sharepoint.com).
    "https://sharepoint.com,"

    // The new branding for Microsoft 365 web apps. Word, PowerPoint and Excel
    // can be accessed under https://word.cloud.microsoft/,
    // https://powerpoint.cloud.microsoft/ and https://excel.cloud.microsoft/
    // respectively.
    "https://cloud.microsoft"};

// Controls whether the PWA manifest on Microsoft 365 Urls should be overridden
// with a static PWA manifest id.
BASE_FEATURE(kMicrosoft365ManifestOverride,
             "Microsoft365ManifestOverride",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Comma separated list of Urls where the M365 PWA manifest should be
// overridden.
const base::FeatureParam<std::string> kMicrosoft365ManifestUrls{
    &kMicrosoft365ManifestOverride, "m365-manifest-urls",
    /*default*/

    // The current Microsoft 365 web app.
    "https://www.microsoft365.com/,"

    // The new branding for the Microsoft 365 web app.
    "https://m365.cloud.microsoft/"};

// Enables the Microsoft OneDrive integration workflow for enterprise users to
// cloud integration support.
BASE_FEATURE(kMicrosoftOneDriveIntegrationForEnterprise,
             "MicrosoftOneDriveIntegrationForEnterprise",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRoundedWindows,
             "RoundedWindows",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables CloudFileSystem for FileSystemProvider extensions.
BASE_FEATURE(kFileSystemProviderCloudFileSystem,
             "FileSystemProviderCloudFileSystem",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a content cache in CloudFileSystem for FileSystemProvider extensions.
BASE_FEATURE(kFileSystemProviderContentCache,
             "FileSystemProviderContentCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables hiding apps disabled by SystemFeaturesDisableList policy by default
// in user sessions.
BASE_FEATURE(kSystemFeaturesDisableListHidden,
             "SystemFeaturesDisableListHidden",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables pinning the NotebookLM preinstalled app to the shelf.
BASE_FEATURE(kNotebookLmAppShelfPin,
             "NotebookLmAppShelfPin",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Resets the act of pinning the NotebookLM preinstalled app to the shelf, used
// for manual testing.
BASE_FEATURE(kNotebookLmAppShelfPinReset,
             "NotebookLmAppShelfPinReset",
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

bool IsCloudGamingDeviceEnabled() {
  return base::FeatureList::IsEnabled(kCloudGamingDevice);
}

bool IsAlmanacLauncherPayloadEnabled() {
  return base::FeatureList::IsEnabled(kAlmanacLauncherPayload);
}

bool IsBlinkExtensionEnabled() {
  return base::FeatureList::IsEnabled(kBlinkExtension);
}

bool IsCrosComponentsEnabled() {
  return base::FeatureList::IsEnabled(kCrosComponents);
}

bool IsCrosMallSwaEnabled() {
  return base::FeatureList::IsEnabled(kCrosMall);
}

bool IsDataControlsFileAccessDefaultDenyEnabled() {
  return base::FeatureList::IsEnabled(kDataControlsFileAccessDefaultDeny);
}

bool IsDataMigrationEnabled() {
  return base::FeatureList::IsEnabled(kDataMigration);
}

bool IsEssentialSearchEnabled() {
  return base::FeatureList::IsEnabled(kEssentialSearch);
}

bool IsFileSystemProviderCloudFileSystemEnabled() {
  return base::FeatureList::IsEnabled(kFileSystemProviderCloudFileSystem);
}

bool IsFileSystemProviderContentCacheEnabled() {
  // The `ContentCache` will be owned by the `CloudFileSystem`. Thus, the
  // `FileSystemProviderCloudFileSystem` flag has to be enabled too.
  if (!IsFileSystemProviderCloudFileSystemEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kFileSystemProviderContentCache);
}

bool IsSystemFeaturesDisableListHiddenEnabled() {
  return base::FeatureList::IsEnabled(kSystemFeaturesDisableListHidden);
}

bool IsGeminiAppPreinstallFeatureManagementEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementGeminiAppPreinstall);
}

bool IsGeminiAppPreinstallEnabled() {
  return base::FeatureList::IsEnabled(kGeminiAppPreinstall);
}

bool IsMagicBoostRevampEnabled() {
  return base::FeatureList::IsEnabled(kMagicBoostRevamp);
}

bool IsMagicBoostRevampForQuickAnswersEnabled() {
  return base::FeatureList::IsEnabled(kMagicBoostRevampForQuickAnswers);
}

bool IsMahiEnabled() {
  return base::FeatureList::IsEnabled(kMahi) &&
         base::FeatureList::IsEnabled(kFeatureManagementMahi);
}

// Mahi requests are composed & sent from ash.
bool IsMahiSendingUrl() {
  return base::FeatureList::IsEnabled(kMahiSendingUrl);
}

bool IsMahiManagedEnabled() {
  return base::FeatureList::IsEnabled(kMahiManaged);
}

bool IsMahiDebuggingEnabled() {
  return base::FeatureList::IsEnabled(kMahiDebugging);
}

bool IsPlatformKeysChangesWave1Enabled() {
  return base::FeatureList::IsEnabled(kPlatformKeysChangesWave1);
}

bool IsPompanoEnabled() {
  return base::FeatureList::IsEnabled(kPompano);
}

bool IsMahiSummarizeSelectedEnabled() {
  return base::FeatureList::IsEnabled(kMahiSummarizeSelected);
}

bool IsNotificationWidthIncreaseEnabled() {
  return base::FeatureList::IsEnabled(kNotificationWidthIncrease);
}

bool IsOfficeNavigationCapturingReimplEnabled() {
  return base::FeatureList::IsEnabled(kOfficeNavigationCapturingReimpl);
}

bool IsOrcaEnabled() {
  return base::FeatureList::IsEnabled(chromeos::features::kOrcaDogfood) ||
         (base::FeatureList::IsEnabled(chromeos::features::kOrca) &&
          base::FeatureList::IsEnabled(kFeatureManagementOrca));
}

bool IsOrcaUseL10nStringsEnabled() {
  return base::FeatureList::IsEnabled(chromeos::features::kOrcaUseL10nStrings);
}

bool IsOrcaInternationalizeEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kOrcaInternationalize);
}

bool ShouldDisableChromeComposeOnChromeOS() {
  return base::FeatureList::IsEnabled(kFeatureManagementDisableChromeCompose) ||
         IsOrcaEnabled();
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
  return base::FeatureList::IsEnabled(kUploadOfficeToCloud);
}

bool IsUploadOfficeToCloudForEnterpriseEnabled() {
  return base::FeatureList::IsEnabled(kUploadOfficeToCloud) &&
         base::FeatureList::IsEnabled(kUploadOfficeToCloudForEnterprise);
}

bool IsUploadOfficeToCloudSyncEnabled() {
  return base::FeatureList::IsEnabled(kUploadOfficeToCloudSync);
}

bool IsMicrosoft365ScopeExtensionsEnabled() {
  return base::FeatureList::IsEnabled(kMicrosoft365ScopeExtensions);
}

bool IsMicrosoft365ManifestOverrideEnabled() {
  return base::FeatureList::IsEnabled(kMicrosoft365ManifestOverride);
}

bool IsMicrosoftOneDriveIntegrationForEnterpriseEnabled() {
  return IsUploadOfficeToCloudEnabled() &&
         base::FeatureList::IsEnabled(
             kMicrosoftOneDriveIntegrationForEnterprise);
}

bool IsRoundedWindowsEnabled() {
  static bool is_enabled =
      base::FeatureList::IsEnabled(kFeatureManagementRoundedWindows) &&
      base::FeatureList::IsEnabled(kRoundedWindows);
  return is_enabled;
}

bool IsSystemBlurEnabled() {
  static bool disable_blur = base::FeatureList::IsEnabled(kDisableSystemBlur);
  return !disable_blur;
}

bool IsPkcs12ToChapsDualWriteEnabled() {
  return base::FeatureList::IsEnabled(kEnablePkcs12ToChapsDualWrite);
}

bool IsFeatureManagementHistoryEmbeddingEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementHistoryEmbedding);
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
