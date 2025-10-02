// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_

#include <iosfwd>
#include <optional>
#include <variant>
#include <vector>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_enums.mojom.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_types.h"

class GURL;

// Different settings that can be assigned for a particular content type.  We
// give the user the ability to set these on a global and per-origin basis.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum ContentSetting {
  CONTENT_SETTING_DEFAULT = 0,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ASK,
  CONTENT_SETTING_SESSION_ONLY,
  CONTENT_SETTING_NUM_SETTINGS
};

// Commonly used setting values for options of permission settings
// Do not modify this enum. If different/additional  states are required, the
// permission should define its own enum.
enum class PermissionOption {
  kAllowed = 1,
  kDenied = 2,
  kAsk = 3,

  kMinValue = kAllowed,
  kMaxValue = kAsk
};

bool IsValidPermissionOption(PermissionOption setting);

struct GeolocationSetting {
  PermissionOption approximate = PermissionOption::kAsk;
  PermissionOption precise = PermissionOption::kAsk;

  auto operator<=>(const GeolocationSetting&) const = default;
};

using PermissionSetting = std::variant<ContentSetting, GeolocationSetting>;

std::ostream& operator<<(std::ostream& os, const GeolocationSetting& it);
std::ostream& operator<<(std::ostream& os, const PermissionSetting& it);
std::ostream& operator<<(std::ostream& os,
                         const std::optional<PermissionSetting>& it);

// Range-checked conversion of an int to a ContentSetting, for use when reading
// prefs off disk.
ContentSetting IntToContentSetting(int content_setting);

struct ContentSettingPatternSource {
  ContentSettingPatternSource(const ContentSettingsPattern& primary_pattern,
                              const ContentSettingsPattern& secondary_patttern,
                              base::Value setting_value,
                              content_settings::mojom::ProviderType provider,
                              bool incognito,
                              content_settings::RuleMetaData metadata =
                                  content_settings::RuleMetaData());
  ContentSettingPatternSource(const ContentSettingPatternSource& other);
  ContentSettingPatternSource();
  ContentSettingPatternSource& operator=(
      const ContentSettingPatternSource& other);
  ~ContentSettingPatternSource();
  ContentSetting GetContentSetting() const;
  bool IsExpired() const;

  bool operator==(const ContentSettingPatternSource& other) const;

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  base::Value setting_value;
  content_settings::RuleMetaData metadata;
  content_settings::mojom::ProviderType source =
      content_settings::mojom::ProviderType::kNone;
  bool incognito;
};

// Formatter method for Google Test.
std::ostream& operator<<(std::ostream& os,
                         const ContentSettingPatternSource& source);

typedef std::vector<ContentSettingPatternSource> ContentSettingsForOneType;

struct RendererContentSettingRules {
  // Returns true if |content_type| is a type that is contained in this class.
  // Any new type added below must also update this method.
  static bool IsRendererContentSetting(ContentSettingsType content_type);

  // Filters all the rules by matching the primary pattern with
  // |outermost_main_frame_url|. Any new type added below that needs to match
  // the primary pattern with the outermost main frame's url should also update
  // this method.
  void FilterRulesByOutermostMainFrameURL(const GURL& outermost_main_frame_url);

  RendererContentSettingRules();
  ~RendererContentSettingRules();
  RendererContentSettingRules(const RendererContentSettingRules& rules);
  RendererContentSettingRules(RendererContentSettingRules&& rules);
  RendererContentSettingRules& operator=(
      const RendererContentSettingRules& rules);
  RendererContentSettingRules& operator=(RendererContentSettingRules&& rules);

  bool operator==(const RendererContentSettingRules& other) const;

  ContentSettingsForOneType mixed_content_rules;
};

namespace content_settings {

using ProviderType = mojom::ProviderType;

// Enum containing the various source for content settings. Settings can be
// set by policy, extension, the user or by the custodian of a supervised user.
// Certain (internal) origins are allowlisted. For these origins the source is
// |SettingSource::kAllowList|.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ContentSettingSource
enum class SettingSource {
  kNone,
  kPolicy,
  kExtension,
  kUser,
  kAllowList,
  kSupervised,
  kInstalledWebApp,
  kTpcdGrant,
  kOsJavascriptOptimizer,
  kTest,
};

// |SettingInfo| provides meta data for content setting values. |source|
// contains the source of a value. |primary_pattern| and |secondary_pattern|
// contains the patterns of the appling rule.
struct SettingInfo {
  SettingInfo();
  SettingInfo(SettingInfo&& other);
  SettingInfo(SettingInfo& other) = delete;
  SettingInfo& operator=(SettingInfo&& other);
  SettingInfo& operator=(const SettingInfo& other) = delete;

  SettingInfo Clone() const;

  SettingSource source = SettingSource::kNone;
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  RuleMetaData metadata;

  void SetAttributes(const content_settings::RuleEntry& rule_entry) {
    primary_pattern = rule_entry.first.primary_pattern;
    secondary_pattern = rule_entry.first.secondary_pattern;
    metadata = rule_entry.second.metadata.Clone();
  }
  void SetAttributes(const ContentSettingPatternSource& content_setting) {
    primary_pattern = content_setting.primary_pattern;
    secondary_pattern = content_setting.secondary_pattern;
    metadata = content_setting.metadata.Clone();
  }
};

// Returns the SettingSource associated with the given ProviderType.
constexpr SettingSource GetSettingSourceFromProviderType(
    ProviderType provider_type) {
  switch (provider_type) {
    case ProviderType::kWebuiAllowlistProvider:
    case ProviderType::kComponentExtensionProvider:
      return SettingSource::kAllowList;
    case ProviderType::kPolicyProvider:
      return SettingSource::kPolicy;
    case ProviderType::kSupervisedProvider:
      return SettingSource::kSupervised;
    case ProviderType::kCustomExtensionProvider:
      return SettingSource::kExtension;
    case ProviderType::kInstalledWebappProvider:
      return SettingSource::kInstalledWebApp;
    case ProviderType::kJavascriptOptimizerAndroidProvider:
      return SettingSource::kOsJavascriptOptimizer;
    case ProviderType::kNotificationAndroidProvider:
    case ProviderType::kOneTimePermissionProvider:
    case ProviderType::kPrefProvider:
    case ProviderType::kDefaultProvider:
      return SettingSource::kUser;
    case ProviderType::kProviderForTests:
    case ProviderType::kOtherProviderForTests:
      return SettingSource::kTest;
    case content_settings::ProviderType::kNone:
      return SettingSource::kNone;
  }
}
}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_
