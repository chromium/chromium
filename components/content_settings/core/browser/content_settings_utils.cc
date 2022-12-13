// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <vector>

#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
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
#if !BUILDFLAG(IS_ANDROID)
  map->GetSettingsForOneType(ContentSettingsType::IMAGES,
                             &(rules->image_rules));
  map->GetSettingsForOneType(ContentSettingsType::MIXEDSCRIPT,
                             &(rules->mixed_content_rules));
  // Auto dark web content settings is available only for Android, so ALLOW rule
  // is added for all origins.
  rules->auto_dark_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingToValue(CONTENT_SETTING_ALLOW), std::string(),
      map->IsOffTheRecord()));
#else
  // Android doesn't use image content settings, so ALLOW rule is added for
  // all origins.
  rules->image_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingToValue(CONTENT_SETTING_ALLOW), std::string(),
      map->IsOffTheRecord()));
  // In Android active mixed content is hard blocked, with no option to allow
  // it.
  rules->mixed_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingToValue(CONTENT_SETTING_BLOCK), std::string(),
      map->IsOffTheRecord()));
  map->GetSettingsForOneType(ContentSettingsType::AUTO_DARK_WEB_CONTENT,
                             &(rules->auto_dark_content_rules));
#endif
  map->GetSettingsForOneType(ContentSettingsType::JAVASCRIPT,
                             &(rules->script_rules));
  map->GetSettingsForOneType(ContentSettingsType::POPUPS,
                             &(rules->popup_redirect_rules));
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
  NOTREACHED();
  return true;
}

// Currently only SessionModel::Durable constraints need to be persistent
// as they are only bounded by time and can persist through multiple browser
// sessions.
bool IsConstraintPersistent(const ContentSettingConstraints& constraints) {
  return constraints.session_model == SessionModel::Durable;
}

// Convenience helper to calculate the expiration time of a constraint given a
// desired |duration|
base::Time GetConstraintExpiration(const base::TimeDelta duration) {
  DCHECK(!duration.is_zero());
  return base::Time::Now() + duration;
}

bool CanTrackLastVisit(ContentSettingsType type) {
#if BUILDFLAG(IS_ANDROID)
  // The notification provider on Android does not support last visit tracking.
  if (type == ContentSettingsType::NOTIFICATIONS)
    return false;
#endif
  // Protocol handler don't actually use their content setting and don't have
  // a valid "initial default" value.
  if (type == ContentSettingsType::PROTOCOL_HANDLERS)
    return false;

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
  if (features::kSafetyCheckUnusedSitePermissionsNoDelay.Get())
    return base::Days(0);
  return base::Days(7);
}

}  // namespace content_settings
