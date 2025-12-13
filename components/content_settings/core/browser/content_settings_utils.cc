// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <array>
#include <vector>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
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
const auto kContentSettingsStringMapping =
    std::to_array<ContentSettingsStringMapping>({
        {CONTENT_SETTING_DEFAULT, "default"},
        {CONTENT_SETTING_ALLOW, "allow"},
        {CONTENT_SETTING_BLOCK, "block"},
        {CONTENT_SETTING_ASK, "ask"},
        {CONTENT_SETTING_SESSION_ONLY, "session_only"},
    });
static_assert(std::size(kContentSettingsStringMapping) ==
                  CONTENT_SETTING_NUM_SETTINGS,
              "kContentSettingsToFromString should have "
              "CONTENT_SETTING_NUM_SETTINGS elements");

// Content settings sorted from most to least permissive. The order is chosen
// to check if a permission grants more rights than another. This is intuitive
// for ALLOW, ASK and BLOCK. SESSION_ONLY belongs between ALLOW and ASK.
// DEFAULT should never be used and is therefore not part of this array.
const ContentSetting kContentSettingOrder[] = {
    // clang-format off
    CONTENT_SETTING_ALLOW,
    CONTENT_SETTING_SESSION_ONLY,
    CONTENT_SETTING_ASK,
    CONTENT_SETTING_BLOCK
    // clang-format on
};

// PermissionOptions sorted from most to least permissive.
const PermissionOption kPermissionOptionOrder[] = {
    // clang-format off
    PermissionOption::kAllowed,
    PermissionOption::kAsk,
    PermissionOption::kDenied,
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
#if !BUILDFLAG(IS_IOS)
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
    if (setting == b) {
      return false;
    }
    if (setting == a) {
      return true;
    }
  }
  NOTREACHED();
}

bool IsMorePermissive(PermissionOption a, PermissionOption b) {
  // Check whether |a| or |b| is reached first in
  // kPermissionOptionOrder. If |a| is first, it means that |a| is more
  // permissive than |b|.
  for (PermissionOption setting : kPermissionOptionOrder) {
    if (setting == b) {
      return false;
    }
    if (setting == a) {
      return true;
    }
  }
  NOTREACHED();
}

// Currently only mojom::SessionModel::DURABLE constraints need to be persistent
// as they are only bounded by time and can persist through multiple browser
// sessions.
bool IsConstraintPersistent(const ContentSettingConstraints& constraints) {
  return constraints.session_model() == mojom::SessionModel::DURABLE;
}

bool CanTrackLastVisit(ContentSettingsType type) {
  DCHECK(WebsiteSettingsRegistry::GetInstance()->Get(type)) << type;

  // Chooser based content settings will not be tracked by default.
  // Only allowlisted ones should be tracked.
  if (IsChooserPermissionEligibleForAutoRevocation(type)) {
    return true;
  }

  auto* info = PermissionSettingsRegistry::GetInstance()->Get(type);
  return info && info->delegate().CanTrackLastVisit();
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

bool IsPermissionEligibleForAutoRevocation(ContentSettingsType type) {
  DCHECK(WebsiteSettingsRegistry::GetInstance()->Get(type)) << type;

  auto* permission_settings_info =
      PermissionSettingsRegistry::GetInstance()->Get(type);
  return (permission_settings_info && CanTrackLastVisit(type)) ||
         IsChooserPermissionEligibleForAutoRevocation(type);
}

bool CanBeAutoRevokedAsUnusedPermission(ContentSettingsType type,
                                        const base::Value& value,
                                        bool is_one_time) {
  DCHECK(WebsiteSettingsRegistry::GetInstance()->Get(type)) << type;

  // The Permissions module in Safety check will revoke permissions after
  // a finite amount of time.
  // We're only interested in expiring permissions that are either
  // A. permission settings (= PermissionSettingsRegistry-based), which
  //    1. Are ALLOWed.
  //    2. Are of eligible ContentSettingsType.
  //      (That includes the default value being ASK. By definition, all
  //      Permissions are ASK by default. If that changes in the future,
  //      consider whether revocation for such permission makes sense. If not,
  //      make sure last_visited is not unnecessarily tracked for them.)
  //    3. Are not already a one-time grant.
  // B. chooser permissions (= WebsiteSettingsRegistry-based), which
  //    1. Are allowlisted.
  //    2. Have a non-empty value.
  if (is_one_time) {
    return false;
  }

  auto* permission_settings_info =
      PermissionSettingsRegistry::GetInstance()->Get(type);
  if (permission_settings_info) {
    auto setting = permission_settings_info->delegate().FromValue(value);
    // If the setting is already DEFAULT or the value is corrupt, no need to
    // revoke the permission.
    if (!setting.has_value()) {
      return false;
    }

    // Currently Safety Check does not store the actual value of a permission
    // and restores all permissions as ALLOW.
    // TODO(crbug.com/441689815): Store PermissionSettings in Safety Check and
    // remove this check.
    if (setting != PermissionSetting{CONTENT_SETTING_ALLOW}) {
      return false;
    }

    return permission_settings_info->delegate().IsAnyPermissionAllowed(
               setting.value()) &&
           CanTrackLastVisit(type);
  } else {
    // If the value is already empty, no need to revoke the permission.
    return IsChooserPermissionEligibleForAutoRevocation(type) &&
           !value.is_none();
  }
}

bool IsChooserPermissionEligibleForAutoRevocation(ContentSettingsType type) {
  // Currently, only File System Access is allowlisted for auto-revoking unused
  // site permissions among chooser-based permissions.
  return type == ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA;
}

const std::vector<ContentSettingsType>& GetTypesWithTemporaryGrants() {
  static base::NoDestructor<const std::vector<ContentSettingsType>> types{{
#if !BUILDFLAG(IS_ANDROID)
      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
      ContentSettingsType::CAPTURED_SURFACE_CONTROL,
#endif
      ContentSettingsType::KEYBOARD_LOCK,
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::HAND_TRACKING,
      ContentSettingsType::SMART_CARD_DATA,
      ContentSettingsType::AR,
      ContentSettingsType::VR,
  }};
  return *types;
}

const std::vector<ContentSettingsType>& GetTypesWithTemporaryGrantsInHcsm() {
  static base::NoDestructor<const std::vector<ContentSettingsType>> types{{
#if !BUILDFLAG(IS_ANDROID)
      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
      ContentSettingsType::CAPTURED_SURFACE_CONTROL,
#endif
      ContentSettingsType::KEYBOARD_LOCK,
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::HAND_TRACKING,
      ContentSettingsType::AR,
      ContentSettingsType::VR,
  }};
  return *types;
}

bool ShouldTypeExpireActively(ContentSettingsType type) {
  return base::FeatureList::IsEnabled(features::kActiveContentSettingExpiry) &&
         base::Contains(GetTypesWithTemporaryGrantsInHcsm(), type);
}

PermissionSetting ValueToPermissionSetting(const PermissionSettingsInfo* info,
                                           const base::Value& value) {
  auto setting = info->delegate().FromValue(value);
  DCHECK(setting.has_value()) << value.DebugString();
  DCHECK(info->delegate().IsValid(*setting)) << value.DebugString();
  return setting.value_or(info->GetInitialDefaultSetting());
}

base::Value PermissionSettingToValue(const PermissionSettingsInfo* info,
                                     const PermissionSetting& setting) {
  DCHECK(info->delegate().IsValid(setting));
  auto value = info->delegate().ToValue(setting);
  return value;
}

}  // namespace content_settings
