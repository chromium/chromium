// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "chromeos_features.h"

namespace chromeos::features {

// Enables smaller battery badge icons to improve legibility of the battery
// percentage.
BASE_FEATURE(kBatteryBadgeIcon, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables better quick settings UI for bluetooth and wifi error states.
BASE_FEATURE(kBluetoothWifiQSPodRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

// System location provider will use caching to optimize GCP usage. This flag
// will be enabled with Finch.
BASE_FEATURE(kCachedLocationProvider, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables cloud game features.
BASE_FEATURE(kCloudGamingDevice, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables MPS to push payload to chrome devices.
BASE_FEATURE(kAlmanacLauncherPayload, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Apps APIs.
BASE_FEATURE(kBlinkExtension, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ChromeOS Kiosk APIs.
BASE_FEATURE(kBlinkExtensionKiosk, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of cros-component UI elements. Contact:
// cros-jellybean-team@google.com.
BASE_FEATURE(kCrosComponents, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the allowlist for the setShape Blink extension for Isolated Web Apps
// on ChromeOS. This is intended to be used as the kill switch for the feature.
BASE_FEATURE(kCrosIsolatedWebAppSetShapeAllowlist,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables denying file access to dlp protected files in MyFiles.
BASE_FEATURE(kDataControlsFileAccessDefaultDeny,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag used to gate preinstallation of the Gemini app.
BASE_FEATURE(kGeminiAppPreinstall, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new Magic Boost Consent Flow.
BASE_FEATURE(kMagicBoostRevamp, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new Magic Boost Consent Flow For Quick Answers.
BASE_FEATURE(kMagicBoostRevampForQuickAnswers,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the mahi feature from the feature management
// module.
BASE_FEATURE(kFeatureManagementMahi, base::FEATURE_DISABLED_BY_DEFAULT);


// Changes the ChromeOS notification width size from 360px to 400px for pop-up
// notifications and 344px to 400px for notifications in the message center.
BASE_FEATURE(kNotificationWidthIncrease, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature.
BASE_FEATURE(kOrca, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature for dogfood population.
BASE_FEATURE(kOrcaDogfood, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature management flag used to gate preinstallation of the Gemini app. This
// flag is meant to be enabled by the feature management module.
BASE_FEATURE(kFeatureManagementGeminiAppPreinstall,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the history embedding feature from the
// feature management module.
BASE_FEATURE(kFeatureManagementHistoryEmbedding,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the passage embedder infrastructure from the
// feature management module.
BASE_FEATURE(kFeatureManagementPassageEmbedder,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls enabling / disabling the orca feature from the feature management
// module.
BASE_FEATURE(kFeatureManagementOrca, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to disable chrome compose.
BASE_FEATURE(kFeatureManagementDisableChromeCompose,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables GLIC on ChromeOS. This flag is intended to be controlled by the
// feature management module.
BASE_FEATURE(kFeatureManagementGlic, base::FEATURE_DISABLED_BY_DEFAULT);

// If true, it enabled GLIC on 8GB devices (or higher) bypassing the CBX device
// requirement.
BASE_FEATURE(kGlicEnableFor8GbDevices, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables rounded windows. This flag is intended to be controlled by the
// feature management module.
BASE_FEATURE(kFeatureManagementRoundedWindows,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the first wave of new features for the chrome.enterprise.platformKeys
// API. That includes:
//   - a new key type (RSA-OAEP) with a new allowed key usage (unwrapKey).
//   - a new API method to `setKeyTag()`, used to mark keys for future lookup.
// Other features might be added in this first wave, or be hold for the second
// wave. For additional details, see crbug.com/288880151.
BASE_FEATURE(kPlatformKeysChangesWave1, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable quick answers V2 settings sub-toggles.
BASE_FEATURE(kQuickAnswersV2SettingsSubToggle,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Quick Answers Rich card.
BASE_FEATURE(kQuickAnswersRichCard, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Material Next UI for Quick Answers.
BASE_FEATURE(kQuickAnswersMaterialNextUI, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables CloudFileSystem for FileSystemProvider extensions.
BASE_FEATURE(kFileSystemProviderCloudFileSystem,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a content cache in CloudFileSystem for FileSystemProvider extensions.
BASE_FEATURE(kFileSystemProviderContentCache,
             base::FEATURE_DISABLED_BY_DEFAULT);


bool IsBatteryBadgeIconEnabled() {
  return base::FeatureList::IsEnabled(kBatteryBadgeIcon);
}

bool IsBluetoothWifiQSPodRefreshEnabled() {
  return base::FeatureList::IsEnabled(kBluetoothWifiQSPodRefresh);
}

bool IsCachedLocationProviderEnabled() {
  return base::FeatureList::IsEnabled(kCachedLocationProvider);
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

bool IsCrosIsolatedWebAppSetShapeAllowlistEnabled() {
  return base::FeatureList::IsEnabled(kCrosIsolatedWebAppSetShapeAllowlist);
}

bool IsDataControlsFileAccessDefaultDenyEnabled() {
  return base::FeatureList::IsEnabled(kDataControlsFileAccessDefaultDeny);
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
  return base::FeatureList::IsEnabled(kFeatureManagementMahi);
}

bool IsPlatformKeysChangesWave1Enabled() {
  return base::FeatureList::IsEnabled(kPlatformKeysChangesWave1);
}

bool IsNotificationWidthIncreaseEnabled() {
  return base::FeatureList::IsEnabled(kNotificationWidthIncrease);
}

bool IsOrcaEnabled() {
  return base::FeatureList::IsEnabled(chromeos::features::kOrcaDogfood) ||
         (base::FeatureList::IsEnabled(chromeos::features::kOrca) &&
          base::FeatureList::IsEnabled(kFeatureManagementOrca));
}

bool ShouldDisableChromeComposeOnChromeOS() {
  return base::FeatureList::IsEnabled(kFeatureManagementDisableChromeCompose) ||
         IsOrcaEnabled();
}

bool IsQuickAnswersMaterialNextUIEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersMaterialNextUI);
}

bool IsQuickAnswersRichCardEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersRichCard);
}

bool IsQuickAnswersV2SettingsSubToggleEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersV2SettingsSubToggle);
}

bool IsRoundedWindowsEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementRoundedWindows);
}

bool IsSystemBlurEnabled() {
  constexpr base::ByteSize kMinimumMemoryThreshold = base::GiB(4);  // 4GB
  return base::SysInfo::AmountOfTotalPhysicalMemory() > kMinimumMemoryThreshold;
}

bool IsFeatureManagementHistoryEmbeddingEnabled() {
  return base::FeatureList::IsEnabled(kFeatureManagementHistoryEmbedding);
}

}  // namespace chromeos::features
