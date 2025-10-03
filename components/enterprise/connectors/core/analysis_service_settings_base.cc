// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/analysis_service_settings_base.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/url_matcher/url_util.h"

namespace enterprise_connectors {

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

void AnalysisServiceSettingsBase::ParsePatternSettings(
    const base::Value::List* pattern_settings_list,
    bool is_enabled_pattern,
    base::MatcherStringPattern::ID& id) {
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
      AddUrlPatternSettings(*pattern_dict, is_enabled_pattern, &id);
    } else if (source_destination_list) {
      AddSourceDestinationSettings(*pattern_dict, is_enabled_pattern, &id);
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
    bool enabled,
    base::MatcherStringPattern::ID* id) {
  DCHECK(id);
  DCHECK(analysis_config_);
  if (enabled) {
    DCHECK(disabled_patterns_settings_.empty());
  } else {
    DCHECK(!enabled_patterns_settings_.empty());
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

  base::MatcherStringPattern::ID previous_id = *id;
  url_matcher::util::AddFiltersWithLimit(matcher_.get(), enabled, id,
                                         *url_list);

  if (previous_id == *id) {
    // No rules were added, so don't save settings, as they would override other
    // valid settings.
    return;
  }

  if (enabled) {
    enabled_patterns_settings_[*id] = std::move(setting);
  } else {
    disabled_patterns_settings_[*id] = std::move(setting);
  }
}

AnalysisServiceSettingsBase::AnalysisServiceSettingsBase() = default;
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
