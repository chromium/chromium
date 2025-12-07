// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_SERVICE_SETTINGS_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_SERVICE_SETTINGS_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

// The settings for an analysis service obtained from a connector policy. This
// class should not be used directly, but rather through its subclasses.
class AnalysisServiceSettingsBase {
 public:
  AnalysisServiceSettingsBase(const AnalysisServiceSettingsBase&) = delete;
  AnalysisServiceSettingsBase(AnalysisServiceSettingsBase&&);
  AnalysisServiceSettingsBase& operator=(const AnalysisServiceSettingsBase&) =
      delete;
  AnalysisServiceSettingsBase& operator=(AnalysisServiceSettingsBase&&);

  virtual ~AnalysisServiceSettingsBase();

  // Returns the analysis settings that apply to the given `url` and
  // `data_region`. Includes cloud analysis settings if the service is
  // configured for cloud analysis. Returns `std::nullopt` if the settings are
  // invalid or no analysis should take place.
  virtual std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      DataRegion data_region) const;

  // Get the block_until_verdict setting if the settings are valid.
  bool ShouldBlockUntilVerdict() const;

  // Get the default_action setting if the settings are valid.
  bool ShouldBlockByDefault() const;

  // Get the custom message/learn more URL. Returns std::nullopt if the
  // settings are invalid or if the message/URL are empty.
  std::optional<std::u16string> GetCustomMessage(const std::string& tag);
  std::optional<GURL> GetLearnMoreUrl(const std::string& tag);
  bool GetBypassJustificationRequired(const std::string& tag);

  std::string service_provider_name() const { return service_provider_name_; }

  // Helpers for convenient check of the underlying variant.
  bool is_cloud_analysis() const;
  bool is_local_analysis() const;

  const AnalysisConfig* GetAnalysisConfig() const { return analysis_config_; }

 protected:
  // The setting to apply when a specific URL pattern is matched.
  struct URLPatternSettings {
    URLPatternSettings();
    URLPatternSettings(const URLPatternSettings&);
    URLPatternSettings(URLPatternSettings&&);
    URLPatternSettings& operator=(const URLPatternSettings&);
    URLPatternSettings& operator=(URLPatternSettings&&);
    ~URLPatternSettings();

    // Tags that correspond to the pattern.
    std::set<std::string> tags;
  };

  // Map from an ID representing a specific matched pattern to its settings.
  using PatternSettings =
      std::map<base::MatcherStringPattern::ID, URLPatternSettings>;

  explicit AnalysisServiceSettingsBase(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);

  // Helper methods for parsing the raw policy settings input
  // Service provider data must be provided and valid
  bool TryParseServiceProviderData(const base::Value::Dict& settings_dict,
                                   const ServiceProviderConfig&);
  void ParseBlockSettings(const base::Value::Dict& settings_dict);
  void ParseMinimumDataSize(const base::Value::Dict& settings_dict);
  void ParseCustomMessages(const base::Value::Dict& settings_dict);
  void ParseJustificationTags(const base::Value::Dict& settings_dict);

  // Returns true if the settings were initialized correctly. If this returns
  // false, then GetAnalysisSettings will always return std::nullopt.
  bool IsValid() const;

  // Returns the common analysis settings with tags for specified matches.
  std::optional<AnalysisSettings> GetCommonAnalysisSettings(
      const std::set<base::MatcherStringPattern::ID>& matches) const;

  // Return tags found in |enabled_patterns_settings| corresponding to the
  // matches while excluding the ones in |disable_patterns_settings|.
  std::map<std::string, TagSettings> GetTags(
      const std::set<base::MatcherStringPattern::ID>& matches) const;

  CloudAnalysisSettings GetCloudAnalysisSettings(DataRegion data_region) const;

  // The next available ID for a settings pattern (e.g. URL,
  // source/destination). This is used to generate unique IDs for patterns as
  // they are added to the appropriate matcher (e.g. URLMatcher). These IDs are
  // then used as keys in the `enabled_patterns_settings_` and
  // `disabled_patterns_settings_` maps.
  base::MatcherStringPattern::ID id_{0};

  // The service provider matching the name given in a Connector policy. nullptr
  // implies that a corresponding service provider doesn't exist and that these
  // settings are not valid.
  raw_ptr<const AnalysisConfig> analysis_config_ = nullptr;

  // The URL matcher created from the patterns set in the analysis policy. The
  // condition set IDs returned after matching against a URL can be used to
  // check |enabled_patterns_settings| and |disable_patterns_settings| to
  // obtain URL-specific settings.
  std::unique_ptr<url_matcher::URLMatcher> matcher_ =
      std::make_unique<url_matcher::URLMatcher>();

  // These members map URL patterns to corresponding settings.  If an entry in
  // the "enabled" or "disabled" lists contains more than one pattern in its
  // "url_list" property, only the last pattern's matcher ID will be added the
  // map.  This keeps the count of these maps smaller and keeps the code from
  // duplicating memory for the settings, which are the same for all URL
  // patterns in a given entry. This optimization works by using
  // std::map::upper_bound to access these maps. The IDs in the disabled
  // settings must be greater than the ones in the enabled settings for this to
  // work and avoid having the two maps cover an overlap of matches.
  PatternSettings enabled_patterns_settings_;
  PatternSettings disabled_patterns_settings_;

  std::string service_provider_name_;

  BlockUntilVerdict block_until_verdict_ = BlockUntilVerdict::kNoBlock;
  DefaultAction default_action_ = DefaultAction::kAllow;
  bool block_password_protected_files_ = false;
  bool block_large_files_ = false;
  size_t minimum_data_size_ = kDefaultMinimumDataSize;
  // A map from tag (dlp, malware, etc) to the custom message, "learn more" link
  // and other settings associated to a specific tag.
  std::map<std::string, TagSettings> tags_;

 private:
  static constexpr size_t kDefaultMinimumDataSize = 100;

  // Accessors for the pattern setting maps.
  static std::optional<URLPatternSettings> GetPatternSettings(
      const PatternSettings& patterns,
      base::MatcherStringPattern::ID match);

  void ParseUrlPatternSettings(const base::Value::List* pattern_settings_list,
                               bool is_enabled_pattern);

  // Updates the states of `matcher_`, `enabled_patterns_settings_` and/or
  // `disabled_patterns_settings_` from a policy value.
  void AddUrlPatternSettings(const base::Value::Dict& url_settings_dict,
                             bool enabled);
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_SERVICE_SETTINGS_BASE_H_
