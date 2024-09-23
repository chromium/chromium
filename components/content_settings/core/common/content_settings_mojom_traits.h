// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_MOJOM_TRAITS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_MOJOM_TRAITS_H_

#include <string>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content_settings::mojom::PatternPartsDataView,
                    ContentSettingsPattern::PatternParts> {
  static const std::string& scheme(
      const ContentSettingsPattern::PatternParts& r) {
    return r.scheme;
  }

  static bool is_scheme_wildcard(
      const ContentSettingsPattern::PatternParts& r) {
    return r.is_scheme_wildcard;
  }

  static const std::string& host(
      const ContentSettingsPattern::PatternParts& r) {
    return r.host;
  }

  static bool has_domain_wildcard(
      const ContentSettingsPattern::PatternParts& r) {
    return r.has_domain_wildcard;
  }

  static const std::string& port(
      const ContentSettingsPattern::PatternParts& r) {
    return r.port;
  }

  static bool is_port_wildcard(const ContentSettingsPattern::PatternParts& r) {
    return r.is_port_wildcard;
  }

  static const std::string& path(
      const ContentSettingsPattern::PatternParts& r) {
    return r.path;
  }

  static bool is_path_wildcard(const ContentSettingsPattern::PatternParts& r) {
    return r.is_path_wildcard;
  }

  static bool Read(content_settings::mojom::PatternPartsDataView data,
                   ContentSettingsPattern::PatternParts* out);
};

template <>
struct StructTraits<content_settings::mojom::ContentSettingsPatternDataView,
                    ContentSettingsPattern> {
  static const ContentSettingsPattern::PatternParts& parts(
      const ContentSettingsPattern& r) {
    return r.parts_;
  }

  static bool is_valid(const ContentSettingsPattern& r) { return r.is_valid_; }

  static bool Read(content_settings::mojom::ContentSettingsPatternDataView data,
                   ContentSettingsPattern* out);
};

template <>
struct EnumTraits<content_settings::mojom::ContentSetting, ContentSetting> {
  static content_settings::mojom::ContentSetting ToMojom(
      ContentSetting setting);

  static bool FromMojom(content_settings::mojom::ContentSetting setting,
                        ContentSetting* out);
};

template <>
struct StructTraits<content_settings::mojom::RuleMetaDataDataView,
                    content_settings::RuleMetaData> {
  static const base::Time& last_modified(
      const content_settings::RuleMetaData& r) {
    return r.last_modified_;
  }

  static const base::Time& last_used(const content_settings::RuleMetaData& r) {
    return r.last_used_;
  }

  static const base::Time& last_visited(
      const content_settings::RuleMetaData& r) {
    return r.last_visited_;
  }

  static const base::Time& expiration(const content_settings::RuleMetaData& r) {
    return r.expiration_;
  }

  static const content_settings::mojom::SessionModel& session_model(
      const content_settings::RuleMetaData& r) {
    return r.session_model_;
  }

  static const base::TimeDelta& lifetime(
      const content_settings::RuleMetaData& r) {
    return r.lifetime_;
  }

  static const content_settings::mojom::TpcdMetadataRuleSource&
  tpcd_metadata_rule_source(const content_settings::RuleMetaData& r) {
    return r.tpcd_metadata_rule_source_;
  }

  static const content_settings::mojom::TpcdMetadataCohort&
  tpcd_metadata_cohort(const content_settings::RuleMetaData& r) {
    return r.tpcd_metadata_cohort_;
  }

  static bool decided_by_related_website_sets(
      const content_settings::RuleMetaData& r) {
    return r.decided_by_related_website_sets_;
  }

  static bool Read(content_settings::mojom::RuleMetaDataDataView data,
                   content_settings::RuleMetaData* out);
};

template <>
struct StructTraits<
    content_settings::mojom::ContentSettingPatternSourceDataView,
    ContentSettingPatternSource> {
  static const ContentSettingsPattern& primary_pattern(
      const ContentSettingPatternSource& r) {
    return r.primary_pattern;
  }

  static const ContentSettingsPattern& secondary_pattern(
      const ContentSettingPatternSource& r) {
    return r.secondary_pattern;
  }

  static const base::Value& setting_value(
      const ContentSettingPatternSource& r) {
    return r.setting_value;
  }

  static const content_settings::RuleMetaData metadata(
      const ContentSettingPatternSource& r) {
    return r.metadata;
  }

  static content_settings::ProviderType source(
      const ContentSettingPatternSource& r) {
    return r.source;
  }

  static bool incognito(const ContentSettingPatternSource& r) {
    return r.incognito;
  }

  static bool Read(
      content_settings::mojom::ContentSettingPatternSourceDataView data,
      ContentSettingPatternSource* out);
};

template <>
struct StructTraits<
    content_settings::mojom::RendererContentSettingRulesDataView,
    RendererContentSettingRules> {
  static const std::vector<ContentSettingPatternSource>& mixed_content_rules(
      const RendererContentSettingRules& r) {
    return r.mixed_content_rules;
  }

  static bool Read(
      content_settings::mojom::RendererContentSettingRulesDataView data,
      RendererContentSettingRules* out);
};

}  // namespace mojo

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_MOJOM_TRAITS_H_
