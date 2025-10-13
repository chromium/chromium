// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_util.h"

#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "components/content_settings/core/common/features.h"
#include "components/safety_check/safety_check.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace safety_hub_util {

base::TimeDelta GetCleanUpThreshold() {
  // TODO(crbug.com/40250875): Clean up delayed clean up logic after the feature
  // is ready. Today, this is necessary to enable manual testing.
  if (content_settings::features::kSafetyCheckUnusedSitePermissionsWithDelay
          .Get()) {
    return safety_hub::kRevocationCleanUpThresholdWithDelayForTesting;
  }
  return safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
}

// TODO(crbug/342210522): Refactor this to be cleaner.
bool IsUrlRevokedAbusiveNotification(HostContentSettingsMap* hcsm,
                                     const GURL& url) {
  DCHECK(url.is_valid());
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
      &info));
  if (stored_value.is_none()) {
    return false;
  }
  DCHECK(stored_value.is_dict());
  std::string setting_val = stored_value.GetDict()
                                .Find(safety_hub::kRevokedStatusDictKeyStr)
                                ->GetString();
  if (setting_val == safety_hub::kRevokeStr) {
    return true;
  }
  DCHECK(setting_val == safety_hub::kIgnoreStr);
  return false;
}

bool IsUrlRevokedUnusedSite(HostContentSettingsMap* hcsm, const GURL& url) {
  DCHECK(url.is_valid());
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &info));
  return !stored_value.is_none();
}

ContentSettingsForOneType GetRevokedAbusiveNotificationPermissions(
    HostContentSettingsMap* hcsm) {
  ContentSettingsForOneType result;
  ContentSettingsForOneType revoked_permissions = hcsm->GetSettingsForOneType(
      ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS);
  // Filter out all `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings whose
  // value specifies to ignore the origin for automatic revocation.
  for (const auto& revoked_permission : revoked_permissions) {
    // This value can be none if it's in the middle of being cleaned up after
    // the threshold has passed.
    base::Value stored_value =
        GetRevokedAbusiveNotificationPermissionsSettingValue(
            hcsm, revoked_permission.primary_pattern.ToRepresentativeUrl());
    if (!stored_value.is_none() &&
        !IsAbusiveNotificationRevocationIgnored(
            hcsm, revoked_permission.primary_pattern.ToRepresentativeUrl())) {
      result.emplace_back(revoked_permission);
    }
  }
  return result;
}

base::Value GetRevokedAbusiveNotificationPermissionsSettingValue(
    HostContentSettingsMap* hcsm,
    GURL setting_url) {
  DCHECK(setting_url.is_valid());
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm->GetWebsiteSetting(
      setting_url, setting_url,
      ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS, &info));
  return stored_value;
}

// TODO(crbug/342210522): Refactor this to be cleaner.
bool IsAbusiveNotificationRevocationIgnored(HostContentSettingsMap* hcsm,
                                            GURL setting_url) {
  DCHECK(setting_url.is_valid());
  base::Value stored_value =
      GetRevokedAbusiveNotificationPermissionsSettingValue(hcsm, setting_url);
  if (stored_value.is_none()) {
    return false;
  }
  // If the REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS dictionary value is
  // set to `safety_hub::kIgnoreStr`, then the user has chosen to ignore
  // revocations for the origin.
  DCHECK(stored_value.GetDict().contains(safety_hub::kRevokedStatusDictKeyStr));
  std::string setting_val = stored_value.GetDict()
                                .Find(safety_hub::kRevokedStatusDictKeyStr)
                                ->GetString();
  if (setting_val == safety_hub::kIgnoreStr) {
    return true;
  }
  DCHECK(setting_val == safety_hub::kRevokeStr);
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
base::Value::Dict GetVersionCardData() {
  base::Value::Dict result;
  switch (g_browser_process->GetBuildState()->update_type()) {
    case BuildState::UpdateType::kNone:
      result.Set(safety_hub::kCardHeaderKey,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_UPDATED));
      result.Set(safety_hub::kCardSubheaderKey,
                 VersionUI::GetAnnotatedVersionStringForUi());
      result.Set(safety_hub::kCardStateKey,
                 static_cast<int>(safety_hub::SafetyHubCardState::kSafe));
      break;
    case BuildState::UpdateType::kNormalUpdate:
    // `kEnterpriseRollback` and `kChannelSwitchRollback` are fairly rare state,
    // they will be handled same as there is waiting updates.
    case BuildState::UpdateType::kEnterpriseRollback:
    case BuildState::UpdateType::kChannelSwitchRollback:
      result.Set(safety_hub::kCardHeaderKey,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_RESTART));
      result.Set(safety_hub::kCardSubheaderKey,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_SUBHEADER_RESTART));
      result.Set(safety_hub::kCardStateKey,
                 static_cast<int>(safety_hub::SafetyHubCardState::kWarning));
  }
  return result;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace safety_hub_util
