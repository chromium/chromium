// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <vector>

#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"

namespace {

const char kPatternSeparator[] = ",";

struct ContentSettingsStringMapping {
  ContentSetting content_setting;
  const char* content_setting_str;
};
const ContentSettingsStringMapping kContentSettingsStringMapping[] = {
    {CONTENT_SETTING_DEFAULT, "default"},
    {CONTENT_SETTING_ALLOW, "allow"},
    {CONTENT_SETTING_BLOCK, "block"},
    {CONTENT_SETTING_ASK, "ask"},
    {CONTENT_SETTING_SESSION_ONLY, "session_only"},
    {CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, "detect_important_content"},
};
static_assert(std::size(kContentSettingsStringMapping) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kContentSettingsToFromString should have "
              "CONTENT_SETTING_NUM_SETTINGS elements");

// Content settings sorted from most to least permissive. The order is chosen
// to check if a permission grants more rights than another. This is intuitive
// for ALLOW, ASK and BLOCK. SESSION_ONLY and DETECT_IMPORTANT_CONTENT are never
// used in the same setting so their respective order is not important but both
// belong between ALLOW and ASK. DEFAULT should never be used and is therefore
// not part of this array.
const ContentSetting kContentSettingOrder[] = {
    // clang-format off
    CONTENT_SETTING_ALLOW,
    CONTENT_SETTING_SESSION_ONLY,
    CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
    CONTENT_SETTING_ASK,
    CONTENT_SETTING_BLOCK
    // clang-format on
};

static_assert(std::size(kContentSettingOrder) ==
                  CONTENT_SETTING_NUM_SETTINGS - 1,
              "kContentSettingOrder should have CONTENT_SETTING_NUM_SETTINGS-1"
              "entries");

}  // namespace

namespace content_settings {

std::string ContentSettingToString(ContentSetting setting) {
  if (setting >= CONTENT_SETTING_DEFAULT &&
      setting < CONTENT_SETTING_NUM_SETTINGS) {
    return kContentSettingsStringMapping[setting].content_setting_str;
  }
  return std::string();
}

bool ContentSettingFromString(const std::string& name,
                              ContentSetting* setting) {
  for (const auto& string_mapping : kContentSettingsStringMapping) {
    if (name == string_mapping.content_setting_str) {
      *setting = string_mapping.content_setting;
      return true;
    }
  }
  *setting = CONTENT_SETTING_DEFAULT;
  return false;
}

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern) {
  return item_pattern.ToString() + std::string(kPatternSeparator) +
         top_level_frame_pattern.ToString();
}

PatternPair ParsePatternString(const std::string& pattern_str) {
  std::vector<std::string> pattern_str_list =
      base::SplitString(pattern_str, std::string(1, kPatternSeparator[0]),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // If the |pattern_str| is an empty string then the |pattern_string_list|
  // contains a single empty string. In this case the empty string will be
  // removed to signal an invalid |pattern_str|. Invalid pattern strings are
  // handle by the "if"-statment below. So the order of the if statements here
  // must be preserved.
  if (pattern_str_list.size() == 1) {
    if (pattern_str_list[0].empty()) {
      pattern_str_list.pop_back();
    } else {
      pattern_str_list.push_back("*");
    }
  }

  if (pattern_str_list.size() > 2 || pattern_str_list.size() == 0) {
    return PatternPair(ContentSettingsPattern(), ContentSettingsPattern());
  }

  PatternPair pattern_pair;
  pattern_pair.first = ContentSettingsPattern::FromString(pattern_str_list[0]);
  pattern_pair.second = ContentSettingsPattern::FromString(pattern_str_list[1]);
  return pattern_pair;
}

void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  rules->mixed_content_rules =
      map->GetSettingsForOneType(ContentSettingsType::MIXEDSCRIPT);
#else
  // In Android active mixed content is hard blocked, with no option to allow
  // it.
  rules->mixed_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingToValue(CONTENT_SETTING_BLOCK), ProviderType::kNone,
      map->IsOffTheRecord()));
#endif
}

bool IsMorePermissive(ContentSetting a, ContentSetting b) {
  // Check whether |a| or |b| is reached first in kContentSettingOrder.
  // If |a| is first, it means that |a| is more permissive than |b|.
  for (ContentSetting setting : kContentSettingOrder) {
    if (setting == b)
      return false;
    if (setting == a)
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return true;
}

// Currently only mojom::SessionModel::DURABLE constraints need to be persistent
// as they are only bounded by time and can persist through multiple browser
// sessions.
bool IsConstraintPersistent(const ContentSettingConstraints& constraints) {
  return constraints.session_model() == mojom::SessionModel::DURABLE;
}

bool CanTrackLastVisit(ContentSettingsType type) {
  // Last visit is not tracked for notification permission as it shouldn't be
  // auto-revoked.
  if (type == ContentSettingsType::NOTIFICATIONS) {
    return false;
  }

  // Protocol handler don't actually use their content setting and don't have
  // a valid "initial default" value.
  if (type == ContentSettingsType::PROTOCOL_HANDLERS) {
    return false;
  }

  // Chooser based content settings will not be tracked by default.
  // Only allowlisted ones should be tracked.
  if (IsChooserPermissionEligibleForAutoRevocation(type)) {
    return true;
  }

  auto* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(type);
  return info && info->GetInitialDefaultSetting() == CONTENT_SETTING_ASK;
}

base::Time GetCoarseVisitedTime(base::Time time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      time.ToDeltaSinceWindowsEpoch().FloorToMultiple(
          GetCoarseVisitedTimePrecision()));
}

base::TimeDelta GetCoarseVisitedTimePrecision() {
  if (features::kSafetyCheckUnusedSitePermissionsNoDelay.Get() ||
      features::kSafetyCheckUnusedSitePermissionsWithDelay.Get()) {
    return base::Days(0);
  }
  return base::Days(7);
}

bool CanBeAutoRevoked(ContentSettingsType type,
                      ContentSetting setting,
                      bool is_one_time) {
  return CanBeAutoRevoked(type, ContentSettingToValue(setting), is_one_time);
}

bool CanBeAutoRevoked(ContentSettingsType type,
                      const base::Value& value,
                      bool is_one_time) {
  // The Permissions module in Safety check will revoke permissions after
  // a finite amount of time.
  // We're only interested in expiring permissions that are either
  // A. regular permissions (= ContentSettingsRegistry-based), which
  //    1. Are ALLOWed.
  //    2. Fall back to ASK.
  //    3. Are not already a one-time grant.
  // B. chooser permissions (= WebsiteSettingsRegistry-based), which
  //    1. Are allowlisted.
  //    2. Have a non-empty value.

  auto* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(type);
  if (info) {
    return !is_one_time &&
           ValueToContentSetting(value) == CONTENT_SETTING_ALLOW &&
           CanTrackLastVisit(type);
  }

  // If the value is already empty, no need to revoke the permission.
  return IsChooserPermissionEligibleForAutoRevocation(type) && !value.is_none();
}

bool IsChooserPermissionEligibleForAutoRevocation(ContentSettingsType type) {
  // Currently, only File System Access is allowlisted for auto-revoking unused
  // site permissions among chooser-based permissions.
  return type == ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA;
}

bool IsGrantedByRelatedWebsiteSets(ContentSettingsType type,
                                   const RuleMetaData& metadata) {
  switch (type) {
    case ContentSettingsType::STORAGE_ACCESS:
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return metadata.decided_by_related_website_sets() ||
             // TODO(b/344678400): Delete after NON_RESTORABLE_USER_SESSION is
             // removed.
             metadata.session_model() ==
                 mojom::SessionModel::NON_RESTORABLE_USER_SESSION;
    default:
      return false;
  }
}

const std::vector<ContentSettingsType>& GetTypesWithTemporaryGrants() {
  static base::NoDestructor<const std::vector<ContentSettingsType>> types{{
#if !BUILDFLAG(IS_ANDROID)
      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
#endif
      ContentSettingsType::KEYBOARD_LOCK,
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::HAND_TRACKING,
      ContentSettingsType::SMART_CARD_DATA,
  }};
  return *types;
}

const std::vector<ContentSettingsType>& GetTypesWithTemporaryGrantsInHcsm() {
  static base::NoDestructor<const std::vector<ContentSettingsType>> types{{
#if !BUILDFLAG(IS_ANDROID)
      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
#endif
      ContentSettingsType::KEYBOARD_LOCK,
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::HAND_TRACKING,
  }};
  return *types;
}

}  // namespace content_settings
