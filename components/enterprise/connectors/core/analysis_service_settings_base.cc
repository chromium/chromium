// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/analysis_service_settings_base.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/url_matcher/url_util.h"

namespace enterprise_connectors {

AnalysisServiceSettingsBase::AnalysisServiceSettingsBase(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict()) {
    return;
  }

  const auto& settings_dict = settings_value.GetDict();

  if (!TryParseServiceProviderData(settings_dict, service_provider_config)) {
    return;
  }

  // Add the url patterns to the settings, which configures settings.matcher and
  // settings.*_pattern_settings. No enable patterns implies the settings are
  // invalid.
  const auto* enabled_pattern_settings_list =
      settings_dict.FindList(kKeyEnable);
  if (!enabled_pattern_settings_list ||
      enabled_pattern_settings_list->empty()) {
    return;
  }

  ParseUrlPatternSettings(enabled_pattern_settings_list, true);
  ParseUrlPatternSettings(settings_dict.FindList(kKeyDisable), false);

  ParseBlockSettings(settings_dict);
  ParseMinimumDataSize(settings_dict);
  ParseCustomMessages(settings_dict);
  ParseJustificationTags(settings_dict);
}

bool AnalysisServiceSettingsBase::TryParseServiceProviderData(
    const base::Value::Dict& settings_dict,
    const ServiceProviderConfig& service_provider_config) {
  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_dict.FindString(kKeyServiceProvider);
  if (!service_provider_name) {
    return false;
  }

  service_provider_name_ = *service_provider_name;
  if (service_provider_config.count(service_provider_name_)) {
    analysis_config_ =
        service_provider_config.at(service_provider_name_).analysis;
  }
  if (!analysis_config_) {
    DLOG(ERROR) << "No analysis config for corresponding service provider";
    return false;
  }

  return true;
}

void AnalysisServiceSettingsBase::ParseUrlPatternSettings(
    const base::Value::List* pattern_settings_list,
    bool is_enabled_pattern) {
  if (!pattern_settings_list || pattern_settings_list->empty()) {
    return;
  }

  for (const base::Value& pattern_setting : *pattern_settings_list) {
    const base::Value::Dict* pattern_dict = pattern_setting.GetIfDict();
    if (!pattern_dict) {
      continue;
    }

    auto* url_list = pattern_dict->FindList(kKeyUrlList);
    auto* source_destination_list =
        pattern_dict->FindList(kKeySourceDestinationList);

    if (url_list && source_destination_list) {
      DLOG(ERROR) << kKeyUrlList << " and " << kKeySourceDestinationList
                  << " specified together. Ignoring it.";
    } else if (url_list) {
      AddUrlPatternSettings(*pattern_dict, is_enabled_pattern);
    }
  }
}

void AnalysisServiceSettingsBase::ParseBlockSettings(
    const base::Value::Dict& settings_dict) {
  // The block settings are optional, so a default is used if they can't be
  // found.
  block_until_verdict_ =
      settings_dict.FindInt(kKeyBlockUntilVerdict).value_or(0)
          ? BlockUntilVerdict::kBlock
          : BlockUntilVerdict::kNoBlock;
  // If fail-closed settings can't be found, the browser defaults to fail open
  // to handle backward compatibility.
  const std::string* default_action_ptr =
      settings_dict.FindString(kKeyDefaultAction);
  default_action_ = default_action_ptr && *default_action_ptr == "block"
                        ? DefaultAction::kBlock
                        : DefaultAction::kAllow;

  block_password_protected_files_ =
      settings_dict.FindBool(kKeyBlockPasswordProtected).value_or(false);
  block_large_files_ =
      settings_dict.FindBool(kKeyBlockLargeFiles).value_or(false);
}

void AnalysisServiceSettingsBase::ParseMinimumDataSize(
    const base::Value::Dict& settings_dict) {
  minimum_data_size_ = settings_dict.FindInt(kKeyMinimumDataSize)
                           .value_or(kDefaultMinimumDataSize);
}

void AnalysisServiceSettingsBase::ParseCustomMessages(
    const base::Value::Dict& settings_dict) {
  const base::Value::List* custom_messages =
      settings_dict.FindList(kKeyCustomMessages);
  if (!custom_messages) {
    return;
  }

  for (const base::Value& value : *custom_messages) {
    const base::Value::Dict& dict = value.GetDict();

    // As of now, this list will contain one message per tag. At some point,
    // the server may start sending one message per language/tag pair. If this
    // is the case, this code should be changed to match the language to
    // Chrome's UI language.
    const std::string* tag = dict.FindString(kKeyCustomMessagesTag);
    if (!tag) {
      continue;
    }

    CustomMessageData data;

    const std::string* message = dict.FindString(kKeyCustomMessagesMessage);
    // This string originates as a protobuf string on the server, which are
    // utf8 and it's used in the UI where it needs to be encoded as utf16. Do
    // the conversion now, otherwise code down the line may not be able to
    // determine if the std::string is ASCII or UTF8 before passing it to the
    // UI.
    data.message = base::UTF8ToUTF16(message ? *message : "");

    const std::string* url = dict.FindString(kKeyCustomMessagesLearnMoreUrl);
    data.learn_more_url = url ? GURL(*url) : GURL();

    tags_[*tag].custom_message = std::move(data);
  }
}

void AnalysisServiceSettingsBase::ParseJustificationTags(
    const base::Value::Dict& settings_dict) {
  const base::Value::List* require_justification_tags =
      settings_dict.FindList(kKeyRequireJustificationTags);
  if (!require_justification_tags) {
    return;
  }

  for (const base::Value& tag : *require_justification_tags) {
    tags_[tag.GetString()].requires_justification = true;
  }
}

void AnalysisServiceSettingsBase::AddUrlPatternSettings(
    const base::Value::Dict& url_settings_dict,
    bool enabled) {
  CHECK(analysis_config_);
  if (enabled) {
    CHECK(disabled_patterns_settings_.empty());
  } else {
    CHECK(!enabled_patterns_settings_.empty());
  }

  URLPatternSettings setting;

  const base::Value::List* tags = url_settings_dict.FindList(kKeyTags);
  if (!tags) {
    return;
  }

  for (const base::Value& tag : *tags) {
    if (tag.is_string()) {
      for (const auto& supported_tag : analysis_config_->supported_tags) {
        if (tag.GetString() == supported_tag.name) {
          setting.tags.insert(tag.GetString());
        }
      }
    }
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value::List* url_list = url_settings_dict.FindList(kKeyUrlList);
  if (!url_list) {
    return;
  }

  base::MatcherStringPattern::ID previous_id = id_;
  url_matcher::util::AddFiltersWithLimit(matcher_.get(), enabled, &id_,
                                         *url_list);

  if (previous_id == id_) {
    // No rules were added, so don't save settings, as they would override other
    // valid settings.
    return;
  }

  if (enabled) {
    enabled_patterns_settings_[id_] = std::move(setting);
  } else {
    disabled_patterns_settings_[id_] = std::move(setting);
  }
}

std::optional<AnalysisSettings>
AnalysisServiceSettingsBase::GetAnalysisSettings(const GURL& url,
                                                 DataRegion data_region) const {
  if (!IsValid()) {
    return std::nullopt;
  }

  CHECK(matcher_);
  auto matches = matcher_->MatchURL(url);
  if (matches.empty()) {
    return std::nullopt;
  }

  auto settings = GetCommonAnalysisSettings(matches);
  if (!settings.has_value() || is_local_analysis()) {
    return settings;
  }

  settings->cloud_or_local_settings =
      CloudOrLocalAnalysisSettings(GetCloudAnalysisSettings(data_region));

  return settings;
}

std::optional<AnalysisSettings>
AnalysisServiceSettingsBase::GetCommonAnalysisSettings(
    const std::set<base::MatcherStringPattern::ID>& matches) const {
  CHECK(IsValid());

  auto tags = GetTags(matches);
  if (tags.empty()) {
    return std::nullopt;
  }

  AnalysisSettings settings;
  settings.block_until_verdict = block_until_verdict_;
  settings.default_action = default_action_;
  settings.block_password_protected_files = block_password_protected_files_;
  settings.block_large_files = block_large_files_;
  settings.minimum_data_size = minimum_data_size_;
  settings.tags = std::move(tags);

  return settings;
}

CloudAnalysisSettings AnalysisServiceSettingsBase::GetCloudAnalysisSettings(
    DataRegion data_region) const {
  CHECK(is_cloud_analysis());

  CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url =
      GetRegionalizedEndpoint(analysis_config_->region_urls, data_region);
  // We assume all support_tags structs have the same max file size.
  cloud_settings.max_file_size =
      analysis_config_->supported_tags[0].max_file_size;
  CHECK(cloud_settings.analysis_url.is_valid());

  return cloud_settings;
}

bool AnalysisServiceSettingsBase::IsValid() const {
  // The settings are invalid if no provider was given.
  if (!analysis_config_) {
    return false;
  }

  // The settings are invalid if no enabled pattern(s) exist since that would
  // imply no URL can ever have an analysis.
  return !enabled_patterns_settings_.empty();
}

std::map<std::string, TagSettings> AnalysisServiceSettingsBase::GetTags(
    const std::set<base::MatcherStringPattern::ID>& matches) const {
  std::set<std::string> enable_tags;
  std::set<std::string> disable_tags;
  for (const base::MatcherStringPattern::ID match : matches) {
    // Enabled patterns need to be checked first, otherwise they always match
    // the first disabled pattern.
    bool enable = true;
    auto maybe_pattern_setting =
        GetPatternSettings(enabled_patterns_settings_, match);
    if (!maybe_pattern_setting.has_value()) {
      maybe_pattern_setting =
          GetPatternSettings(disabled_patterns_settings_, match);
      enable = false;
    }

    CHECK(maybe_pattern_setting.has_value());
    auto tags = std::move(maybe_pattern_setting.value().tags);
    if (enable) {
      enable_tags.insert(tags.begin(), tags.end());
    } else {
      disable_tags.insert(tags.begin(), tags.end());
    }
  }

  for (const std::string& tag_to_disable : disable_tags) {
    enable_tags.erase(tag_to_disable);
  }

  std::map<std::string, TagSettings> output;
  for (const std::string& tag : enable_tags) {
    if (tags_.count(tag)) {
      output[tag] = tags_.at(tag);
    } else {
      output[tag] = TagSettings();
    }
  }

  return output;
}

// static
std::optional<AnalysisServiceSettingsBase::URLPatternSettings>
AnalysisServiceSettingsBase::GetPatternSettings(
    const PatternSettings& patterns,
    base::MatcherStringPattern::ID match) {
  // If the pattern exists directly in the map, return its settings.
  if (patterns.count(match) == 1) {
    return patterns.at(match);
  }

  // If the pattern doesn't exist in the map, it might mean that it wasn't the
  // only pattern to correspond to its settings and that the ID added to
  // the map was the one of the last pattern corresponding to those settings.
  // This means the next match ID greater than |match| has the correct
  // settings if it exists.
  auto next = patterns.upper_bound(match);
  if (next != patterns.end()) {
    return next->second;
  }

  return std::nullopt;
}

bool AnalysisServiceSettingsBase::ShouldBlockUntilVerdict() const {
  if (!IsValid()) {
    return false;
  }

  return block_until_verdict_ == BlockUntilVerdict::kBlock;
}

bool AnalysisServiceSettingsBase::ShouldBlockByDefault() const {
  if (!IsValid()) {
    return false;
  }

  return default_action_ == DefaultAction::kBlock;
}

std::optional<std::u16string> AnalysisServiceSettingsBase::GetCustomMessage(
    const std::string& tag) {
  const auto& element = tags_.find(tag);

  if (!IsValid() || element == tags_.end() ||
      element->second.custom_message.message.empty()) {
    return std::nullopt;
  }

  return element->second.custom_message.message;
}

std::optional<GURL> AnalysisServiceSettingsBase::GetLearnMoreUrl(
    const std::string& tag) {
  const auto& element = tags_.find(tag);

  if (!IsValid() || element == tags_.end() ||
      element->second.custom_message.learn_more_url.is_empty()) {
    return std::nullopt;
  }

  return element->second.custom_message.learn_more_url;
}

bool AnalysisServiceSettingsBase::GetBypassJustificationRequired(
    const std::string& tag) {
  return tags_.find(tag) != tags_.end() && tags_.at(tag).requires_justification;
}

bool AnalysisServiceSettingsBase::is_cloud_analysis() const {
  return analysis_config_ && analysis_config_->url != nullptr;
}

bool AnalysisServiceSettingsBase::is_local_analysis() const {
  return analysis_config_ && analysis_config_->local_path != nullptr;
}

AnalysisServiceSettingsBase::AnalysisServiceSettingsBase(
    AnalysisServiceSettingsBase&&) = default;
AnalysisServiceSettingsBase& AnalysisServiceSettingsBase::operator=(
    AnalysisServiceSettingsBase&&) = default;
AnalysisServiceSettingsBase::~AnalysisServiceSettingsBase() = default;

AnalysisServiceSettingsBase::URLPatternSettings::URLPatternSettings() = default;
AnalysisServiceSettingsBase::URLPatternSettings::URLPatternSettings(
    const AnalysisServiceSettingsBase::URLPatternSettings&) = default;
AnalysisServiceSettingsBase::URLPatternSettings::URLPatternSettings(
    AnalysisServiceSettingsBase::URLPatternSettings&&) = default;
AnalysisServiceSettingsBase::URLPatternSettings&
AnalysisServiceSettingsBase::URLPatternSettings::operator=(
    const AnalysisServiceSettingsBase::URLPatternSettings&) = default;
AnalysisServiceSettingsBase::URLPatternSettings&
AnalysisServiceSettingsBase::URLPatternSettings::operator=(
    AnalysisServiceSettingsBase::URLPatternSettings&&) = default;
AnalysisServiceSettingsBase::URLPatternSettings::~URLPatternSettings() =
    default;

}  // namespace enterprise_connectors
