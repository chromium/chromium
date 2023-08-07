// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos::features {

// Enables or disables more filtering out of phones from the Bluetooth UI.
BASE_FEATURE(kBluetoothPhoneFilter,
             "BluetoothPhoneFilter",
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

// Enables the use of cros-component UI elements. Contact:
// cros-jellybean-team@google.com.
BASE_FEATURE(kCrosComponents,
             "CrosComponents",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enable experimental goldfish web app profile isolation.
BASE_FEATURE(kExperimentalWebAppProfileIsolation,
             "ExperimentalWebAppProfileIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable experimental goldfish web app isolation.
BASE_FEATURE(kExperimentalWebAppStoragePartitionIsolation,
             "ExperimentalWebAppStoragePartitionIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable IWA support for Telemetry Extension API.
BASE_FEATURE(kIWAForTelemetryExtensionAPI,
             "IWAForTelemetryExtensionAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Jelly features. go/jelly-flags
BASE_FEATURE(kJelly, "Jelly", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Jellyroll features. Jellyroll is a feature flag for CrOSNext, which
// controls all system UI updates and new system components. go/jelly-flags
BASE_FEATURE(kJellyroll, "Jellyroll", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable quick answers V2 settings sub-toggles.
BASE_FEATURE(kQuickAnswersV2SettingsSubToggle,
             "QuickAnswersV2SettingsSubToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Quick Answers Rich card.
BASE_FEATURE(kQuickAnswersRichCard,
             "QuickAnswersRichCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Office files upload workflow to improve Office files support.
BASE_FEATURE(kUploadOfficeToCloud,
             "UploadOfficeToCloud",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsClipboardHistoryRefreshEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->EnableClipboardHistoryRefresh();
#else
  return base::FeatureList::IsEnabled(kClipboardHistoryRefresh) &&
         IsJellyEnabled();
#endif
}

BASE_FEATURE(kRoundedWindows,
             "RoundedWindows",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kRoundedWindowsRadius[] = "window_radius";

bool IsCloudGamingDeviceEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsCloudGamingDevice();
#else
  return base::FeatureList::IsEnabled(kCloudGamingDevice);
#endif
}

bool IsCrosComponentsEnabled() {
  return base::FeatureList::IsEnabled(kCrosComponents) && IsJellyEnabled();
}

bool IsIWAForTelemetryExtensionAPIEnabled() {
  return base::FeatureList::IsEnabled(kIWAForTelemetryExtensionAPI);
}

bool IsJellyEnabled() {
  return base::FeatureList::IsEnabled(kJelly);
}

bool IsJellyrollEnabled() {
  // Only enable Jellyroll if Jelly is also enabled as this is how tests expect
  // this to behave.
  return IsJellyEnabled() && base::FeatureList::IsEnabled(kJellyroll);
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

bool IsRoundedWindowsEnabled() {
  // Rounded windows are under the Jelly feature.
  return base::FeatureList::IsEnabled(kRoundedWindows) &&
         base::FeatureList::IsEnabled(kJelly);
}

int RoundedWindowsRadius() {
  if (!IsRoundedWindowsEnabled()) {
    return 0;
  }

  return base::GetFieldTrialParamByFeatureAsInt(
      kRoundedWindows, kRoundedWindowsRadius, /*default_value=*/8);
}

}  // namespace chromeos::features
