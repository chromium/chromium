// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_uma_util.h"

#include <functional>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_uma_constants.h"

namespace {

std::string GetProviderNameForHistograms(
    content_settings::ProviderType provider_type) {
  using ProviderType = content_settings::ProviderType;

  switch (provider_type) {
    // Update the `ContentAllProviderTypes` variants in
    // https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/metadata/content/histograms.xml
    // when new providers are added.
    case ProviderType::kWebuiAllowlistProvider:
      return "WebuiAllowlistProvider";
    case ProviderType::kComponentExtensionProvider:
      return "ComponentExtensionProvider";
    case ProviderType::kPolicyProvider:
      return "PolicyProvider";
    case ProviderType::kSupervisedProvider:
      return "SupervisedProvider";
    case ProviderType::kCustomExtensionProvider:
      return "CustomExtensionProvider";
    case ProviderType::kInstalledWebappProvider:
      return "InstalledWebappProvider";
    case ProviderType::kJavascriptOptimizerAndroidProvider:
      return "JavascriptOptimizerAndroidProvider";
    case ProviderType::kNotificationAndroidProvider:
      return "NotificationAndroidProvider";
    case ProviderType::kOneTimePermissionProvider:
      return "OneTimePermissionProvider";
    case ProviderType::kPrefProvider:
      return "PrefProvider";
    case ProviderType::kExtensionInstallTimePermissionProvider:
      return "ExtensionInstallTimePermissionProvider";
    case ProviderType::kDefaultProvider:
      return "DefaultProvider";
    case ProviderType::kProviderForTests:
      return "ProviderForTests";
    case ProviderType::kOtherProviderForTests:
      return "OtherProviderForTests";
    case ProviderType::kNone:
      NOTREACHED();
  }
}

}  // namespace

namespace content_settings_uma_util {

void RecordContentSettingsHistogram(const std::string& name,
                                    ContentSettingsType content_setting) {
  base::UmaHistogramExactLinear(
      name, ContentSettingTypeToHistogramValue(content_setting),
      content_settings::kContentSettingsTypeToHistogramValueMax + 1);
}

int ContentSettingTypeToHistogramValue(ContentSettingsType content_setting) {
  static_assert(
      content_settings::kContentSettingsTypeToHistogramValue.size() ==
          // DEFAULT is not in the histogram, so we want [0, kMaxValue]
          1 + static_cast<size_t>(ContentSettingsType::kMaxValue),
      "Update content settings histogram lookup");

  auto found = content_settings::kContentSettingsTypeToHistogramValue.find(
      content_setting);
  if (found != content_settings::kContentSettingsTypeToHistogramValue.end()) {
    DCHECK_NE(found->second, -1)
        << "Used for deprecated settings: " << static_cast<int>(found->first);
    return found->second;
  }
  NOTREACHED();
}

void RecordActiveExpiryEvent(content_settings::ProviderType provider_type,
                             ContentSettingsType content_setting_type) {
  content_settings_uma_util::RecordContentSettingsHistogram(
      base::StrCat({"ContentSettings.ActiveExpiry.",
                    GetProviderNameForHistograms(provider_type),
                    ".ContentSettingsType"}),
      content_setting_type);
}

void RecordContentSettingChange(ContentSetting content_setting_value,
                                ContentSettingsType content_setting_type) {
  switch (content_setting_value) {
    case CONTENT_SETTING_ALLOW:
      content_settings_uma_util::RecordContentSettingsHistogram(
          "Permissions.SiteSettingsChanged.Allow", content_setting_type);
      break;
    case CONTENT_SETTING_BLOCK:
      content_settings_uma_util::RecordContentSettingsHistogram(
          "Permissions.SiteSettingsChanged.Block", content_setting_type);
      break;
    case CONTENT_SETTING_DEFAULT:
      content_settings_uma_util::RecordContentSettingsHistogram(
          "Permissions.SiteSettingsChanged.Default", content_setting_type);
      break;
    case CONTENT_SETTING_ASK:
      content_settings_uma_util::RecordContentSettingsHistogram(
          "Permissions.SiteSettingsChanged.Ask", content_setting_type);
      break;
    case CONTENT_SETTING_SESSION_ONLY:
      content_settings_uma_util::RecordContentSettingsHistogram(
          "Permissions.SiteSettingsChanged.SessionOnly", content_setting_type);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace content_settings_uma_util
