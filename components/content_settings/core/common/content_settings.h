// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_types.h"

// Different settings that can be assigned for a particular content type.  We
// give the user the ability to set these on a global and per-origin basis.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ContentSettingValues
//
// TODO(nigeltao): migrate the Java users of this enum to the mojom-generated
// enum.
enum ContentSetting {
  CONTENT_SETTING_DEFAULT = 0,
  CONTENT_SETTING_ALLOW,
  CONTENT_SETTING_BLOCK,
  CONTENT_SETTING_ASK,
  CONTENT_SETTING_SESSION_ONLY,
  CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
  CONTENT_SETTING_NUM_SETTINGS
};

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
enum class SettingSource {
  kNone,
  kPolicy,
  kExtension,
  kUser,
  kAllowList,
  kSupervised,
  kInstalledWebApp,
  kTpcdGrant,
};

// |SettingInfo| provides meta data for content setting values. |source|
// contains the source of a value. |primary_pattern| and |secondary_pattern|
// contains the patterns of the appling rule.
struct SettingInfo {
  SettingSource source = SettingSource::kNone;
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  RuleMetaData metadata;

  void SetAttributes(const content_settings::RuleEntry& rule_entry) {
    primary_pattern = rule_entry.first.primary_pattern;
    secondary_pattern = rule_entry.first.secondary_pattern;
    metadata = rule_entry.second.metadata;
  }
  void SetAttributes(const ContentSettingPatternSource& content_setting) {
    primary_pattern = content_setting.primary_pattern;
    secondary_pattern = content_setting.secondary_pattern;
    metadata = content_setting.metadata;
  }
};

// Returns the SettingSource associated with the given ProviderType.
constexpr SettingSource GetSettingSourceFromProviderType(
    ProviderType provider_type) {
  switch (provider_type) {
    case ProviderType::kWebuiAllowlistProvider:
      return SettingSource::kAllowList;
    case ProviderType::kPolicyProvider:
      return SettingSource::kPolicy;
    case ProviderType::kSupervisedProvider:
      return SettingSource::kSupervised;
    case ProviderType::kCustomExtensionProvider:
      return SettingSource::kExtension;
    case ProviderType::kInstalledWebappProvider:
      return SettingSource::kInstalledWebApp;
    case ProviderType::kNotificationAndroidProvider:
    case ProviderType::kOneTimePermissionProvider:
    case ProviderType::kPrefProvider:
    case ProviderType::kDefaultProvider:
    case ProviderType::kProviderForTests:
    case ProviderType::kOtherProviderForTests:
      return SettingSource::kUser;
    case content_settings::ProviderType::kNone:
      return SettingSource::kNone;
  }
}
}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_H_
