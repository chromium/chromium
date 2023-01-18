// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/chrome_variations_configuration.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_configurations.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_configurations.h"

namespace {

const char kComparatorTypeAny[] = "any";
const char kComparatorTypeLessThan[] = "<";
const char kComparatorTypeGreaterThan[] = ">";
const char kComparatorTypeLessThanOrEqual[] = "<=";
const char kComparatorTypeGreaterThanOrEqual[] = ">=";
const char kComparatorTypeEqual[] = "==";
const char kComparatorTypeNotEqual[] = "!=";

const char kImpactedFeaturesTypeAll[] = "all";
const char kImpactedFeaturesTypeNone[] = "none";

const char kEventConfigUsedKey[] = "event_used";
const char kEventConfigTriggerKey[] = "event_trigger";
const char kEventConfigKeyPrefix[] = "event_";
const char kSessionRateKey[] = "session_rate";
const char kSessionRateImpactKey[] = "session_rate_impact";
const char kBlockingKey[] = "blocking";
const char kBlockedByKey[] = "blocked_by";
const char kAvailabilityKey[] = "availability";
const char kTrackingOnlyKey[] = "tracking_only";
const char kGroupsKey[] = "groups";
const char kIgnoredKeyPrefix[] = "x_";

const char kSnoozeParams[] = "snooze_params";
const char kSnoozeParamsMaxLimit[] = "max_limit";
const char kSnoozeParamsInterval[] = "snooze_interval";

const char kEventConfigDataNameKey[] = "name";
const char kEventConfigDataComparatorKey[] = "comparator";
const char kEventConfigDataWindowKey[] = "window";
const char kEventConfigDataStorageKey[] = "storage";

const char kTrackingOnlyTrue[] = "true";
const char kTrackingOnlyFalse[] = "false";
}  // namespace

namespace feature_engagement {

namespace {

bool ParseComparatorSubstring(const base::StringPiece& definition,
                              Comparator* comparator,
                              ComparatorType type,
                              uint32_t type_len) {
  base::StringPiece number_string =
      base::TrimWhitespaceASCII(definition.substr(type_len), base::TRIM_ALL);
  uint32_t value;
  if (!base::StringToUint(number_string, &value))
    return false;

  comparator->type = type;
  comparator->value = value;
  return true;
}

bool ParseComparator(const base::StringPiece& definition,
                     Comparator* comparator) {
  if (base::EqualsCaseInsensitiveASCII(definition, kComparatorTypeAny)) {
    comparator->type = ANY;
    comparator->value = 0;
    return true;
  }

  if (base::StartsWith(definition, kComparatorTypeLessThanOrEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, LESS_THAN_OR_EQUAL,
                                    2);
  }

  if (base::StartsWith(definition, kComparatorTypeGreaterThanOrEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator,
                                    GREATER_THAN_OR_EQUAL, 2);
  }

  if (base::StartsWith(definition, kComparatorTypeEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, EQUAL, 2);
  }

  if (base::StartsWith(definition, kComparatorTypeNotEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, NOT_EQUAL, 2);
  }

  if (base::StartsWith(definition, kComparatorTypeLessThan,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, LESS_THAN, 1);
  }

  if (base::StartsWith(definition, kComparatorTypeGreaterThan,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, GREATER_THAN, 1);
  }

  return false;
}

bool ParseEventConfig(const base::StringPiece& definition,
                      EventConfig* event_config) {
  // Support definitions with at least 4 tokens.
  auto tokens = base::SplitStringPiece(definition, ";", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
  if (tokens.size() < 4) {
    *event_config = EventConfig();
    return false;
  }

  // Parse tokens in any order.
  bool has_name = false;
  bool has_comparator = false;
  bool has_window = false;
  bool has_storage = false;
  for (const auto& token : tokens) {
    auto pair = base::SplitStringPiece(token, ":", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
    if (pair.size() != 2) {
      *event_config = EventConfig();
      return false;
    }

    const base::StringPiece& key = pair[0];
    const base::StringPiece& value = pair[1];
    // TODO(nyquist): Ensure that key matches regex /^[a-zA-Z0-9-_]+$/.

    if (base::EqualsCaseInsensitiveASCII(key, kEventConfigDataNameKey)) {
      if (has_name) {
        *event_config = EventConfig();
        return false;
      }
      has_name = true;

      event_config->name = std::string(value);
    } else if (base::EqualsCaseInsensitiveASCII(
                   key, kEventConfigDataComparatorKey)) {
      if (has_comparator) {
        *event_config = EventConfig();
        return false;
      }
      has_comparator = true;

      Comparator comparator;
      if (!ParseComparator(value, &comparator)) {
        *event_config = EventConfig();
        return false;
      }

      event_config->comparator = comparator;
    } else if (base::EqualsCaseInsensitiveASCII(key,
                                                kEventConfigDataWindowKey)) {
      if (has_window) {
        *event_config = EventConfig();
        return false;
      }
      has_window = true;

      uint32_t parsed_value;
      if (!base::StringToUint(value, &parsed_value)) {
        *event_config = EventConfig();
        return false;
      }

      event_config->window = parsed_value;
    } else if (base::EqualsCaseInsensitiveASCII(key,
                                                kEventConfigDataStorageKey)) {
      if (has_storage) {
        *event_config = EventConfig();
        return false;
      }
      has_storage = true;

      uint32_t parsed_value;
      if (!base::StringToUint(value, &parsed_value)) {
        *event_config = EventConfig();
        return false;
      }

      event_config->storage = parsed_value;
    }
  }

  return has_name && has_comparator && has_window && has_storage;
}

bool IsKnownFeature(const base::StringPiece& feature_name,
                    const FeatureVector& features) {
  for (const auto* feature : features) {
    if (feature->name == feature_name)
      return true;
  }
  return false;
}

bool IsKnownGroup(const base::StringPiece& group_name,
                  const GroupVector& groups) {
  for (const auto* group : groups) {
    if (group->name == group_name) {
      return true;
    }
  }
  return false;
}

bool ParseSessionRateImpact(const base::StringPiece& definition,
                            SessionRateImpact* session_rate_impact,
                            const base::Feature* this_feature,
                            const FeatureVector& all_features) {
  base::StringPiece trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0)
    return false;

  if (base::EqualsCaseInsensitiveASCII(trimmed_def, kImpactedFeaturesTypeAll)) {
    session_rate_impact->type = SessionRateImpact::Type::ALL;
    return true;
  }

  if (base::EqualsCaseInsensitiveASCII(trimmed_def,
                                       kImpactedFeaturesTypeNone)) {
    session_rate_impact->type = SessionRateImpact::Type::NONE;
    return true;
  }

  auto parsed_feature_names = base::SplitStringPiece(
      trimmed_def, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parsed_feature_names.empty())
    return false;

  std::vector<std::string> affected_features;
  for (const auto& feature_name : parsed_feature_names) {
    if (feature_name.length() == 0) {
      DVLOG(1) << "Empty feature name when parsing session_rate_impact "
               << "for feature " << this_feature->name;
      continue;
    }
    if (base::EqualsCaseInsensitiveASCII(feature_name,
                                         kImpactedFeaturesTypeAll) ||
        base::EqualsCaseInsensitiveASCII(feature_name,
                                         kImpactedFeaturesTypeNone)) {
      DVLOG(1) << "Illegal feature name when parsing session_rate_impact "
               << "for feature " << this_feature->name << ": " << feature_name;
      return false;
    }
    if (!IsKnownFeature(feature_name, all_features)) {
      DVLOG(1) << "Unknown feature name found when parsing session_rate_impact "
               << "for feature " << this_feature->name << ": " << feature_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::
              FAILURE_SESSION_RATE_IMPACT_UNKNOWN_FEATURE);
      continue;
    }
    affected_features.push_back(std::string(feature_name));
  }

  if (affected_features.empty())
    return false;

  session_rate_impact->type = SessionRateImpact::Type::EXPLICIT;
  session_rate_impact->affected_features = std::move(affected_features);
  return true;
}

bool ParseBlockedBy(const base::StringPiece& definition,
                    BlockedBy* blocked_by,
                    const base::Feature* this_feature,
                    const FeatureVector& all_features) {
  base::StringPiece trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0)
    return false;

  if (base::EqualsCaseInsensitiveASCII(trimmed_def, kImpactedFeaturesTypeAll)) {
    blocked_by->type = BlockedBy::Type::ALL;
    return true;
  }

  if (base::EqualsCaseInsensitiveASCII(trimmed_def,
                                       kImpactedFeaturesTypeNone)) {
    blocked_by->type = BlockedBy::Type::NONE;
    return true;
  }

  auto parsed_feature_names = base::SplitStringPiece(
      trimmed_def, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parsed_feature_names.empty())
    return false;

  std::vector<std::string> affected_features;
  for (const auto& feature_name : parsed_feature_names) {
    if (feature_name.length() == 0) {
      DVLOG(1) << "Empty feature name when parsing blocked_by "
               << "for feature " << this_feature->name;
      continue;
    }
    if (base::EqualsCaseInsensitiveASCII(feature_name,
                                         kImpactedFeaturesTypeAll) ||
        base::EqualsCaseInsensitiveASCII(feature_name,
                                         kImpactedFeaturesTypeNone)) {
      DVLOG(1) << "Illegal feature name when parsing blocked_by "
               << "for feature " << this_feature->name << ": " << feature_name;
      return false;
    }
    if (!IsKnownFeature(feature_name, all_features)) {
      DVLOG(1) << "Unknown feature name found when parsing blocked_by "
               << "for feature " << this_feature->name << ": " << feature_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::FAILURE_BLOCKED_BY_UNKNOWN_FEATURE);
      continue;
    }
    affected_features.emplace_back(std::string(feature_name));
  }

  if (affected_features.empty())
    return false;

  blocked_by->type = BlockedBy::Type::EXPLICIT;
  blocked_by->affected_features = std::move(affected_features);
  return true;
}

bool ParseBlocking(const base::StringPiece& definition, Blocking* blocking) {
  base::StringPiece trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0)
    return false;

  if (base::EqualsCaseInsensitiveASCII(trimmed_def, kImpactedFeaturesTypeAll)) {
    blocking->type = Blocking::Type::ALL;
    return true;
  }

  if (base::EqualsCaseInsensitiveASCII(trimmed_def,
                                       kImpactedFeaturesTypeNone)) {
    blocking->type = Blocking::Type::NONE;
    return true;
  }

  return false;
}

bool ParseSnoozeParams(const base::StringPiece& definition,
                       SnoozeParams* snooze_params) {
  auto tokens = base::SplitStringPiece(definition, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
  if (tokens.size() != 2)
    return false;

  bool has_max_limit = false;
  bool has_snooze_interval = false;
  for (const auto& token : tokens) {
    auto pair = base::SplitStringPiece(token, ":", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);

    if (pair.size() != 2)
      return false;

    const base::StringPiece& key = pair[0];
    const base::StringPiece& value = pair[1];
    if (base::EqualsCaseInsensitiveASCII(key, kSnoozeParamsMaxLimit)) {
      uint32_t parsed_value;
      if (!base::StringToUint(value, &parsed_value)) {
        snooze_params->snooze_interval = 0u;
        return false;
      }
      snooze_params->max_limit = parsed_value;
      has_max_limit = true;
    } else if (base::EqualsCaseInsensitiveASCII(key, kSnoozeParamsInterval)) {
      uint32_t parsed_value;
      if (!base::StringToUint(value, &parsed_value)) {
        snooze_params->max_limit = 0u;
        return false;
      }
      snooze_params->snooze_interval = parsed_value;
      has_snooze_interval = true;
    }
  }

  return has_max_limit && has_snooze_interval;
}

bool ParseTrackingOnly(const base::StringPiece& definition,
                       bool* tracking_only) {
  // Since |tracking_only| is a primitive, ensure it set.
  *tracking_only = false;

  base::StringPiece trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (base::EqualsCaseInsensitiveASCII(trimmed_def, kTrackingOnlyTrue)) {
    *tracking_only = true;
    return true;
  }

  return base::EqualsCaseInsensitiveASCII(trimmed_def, kTrackingOnlyFalse);
}

bool ParseGroups(const base::StringPiece& definition,
                 std::vector<std::string>* groups,
                 const base::Feature* this_feature,
                 const GroupVector& all_groups) {
  base::StringPiece trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0) {
    return false;
  }

  auto parsed_group_names = base::SplitStringPiece(
      trimmed_def, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parsed_group_names.empty()) {
    return false;
  }

  for (const auto& group_name : parsed_group_names) {
    if (group_name.length() == 0) {
      DVLOG(1) << "Empty group name when parsing groups "
               << "for feature " << this_feature->name;
      continue;
    }
    if (!IsKnownGroup(group_name, all_groups)) {
      DVLOG(1) << "Unknown group name found when parsing groups "
               << "for feature " << this_feature->name << ": " << group_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::FAILURE_GROUPS_UNKNOWN_GROUP);
      continue;
    }
    groups->emplace_back(std::string(group_name));
  }

  if (groups->empty()) {
    return false;
  }

  return true;
}

// Holds all the possible fields that can be parsed. The parsing code will fill
// the provided items with parsed data. If any field is null, then it won't be
// parsed.
struct ConfigParseOutput {
  uint32_t& parse_errors;
  raw_ptr<Comparator> session_rate = nullptr;
  raw_ptr<SessionRateImpact> session_rate_impact = nullptr;
  raw_ptr<Blocking> blocking = nullptr;
  raw_ptr<BlockedBy> blocked_by = nullptr;
  raw_ptr<EventConfig> trigger = nullptr;
  raw_ptr<EventConfig> used = nullptr;
  raw_ptr<std::set<EventConfig>> event_configs = nullptr;
  raw_ptr<bool> tracking_only = nullptr;
  raw_ptr<Comparator> availability = nullptr;
  raw_ptr<SnoozeParams> snooze_params = nullptr;
  raw_ptr<std::vector<std::string>> groups = nullptr;

  explicit ConfigParseOutput(uint32_t& parse_errors)
      : parse_errors(parse_errors) {}
};

void ParseConfigFields(const base::Feature* feature,
                       const FeatureVector& all_features,
                       const GroupVector& all_groups,
                       std::map<std::string, std::string> params,
                       ConfigParseOutput& output) {
  for (const auto& it : params) {
    std::string param_name = it.first;
    std::string param_value = params[param_name];
    std::string key = param_name;
    // The param name might have a prefix containing the feature name with
    // a trailing underscore, e.g. IPH_FooFeature_session_rate. Strip out
    // the feature prefix for further comparison.
    if (base::StartsWith(key, feature->name, base::CompareCase::SENSITIVE))
      key = param_name.substr(strlen(feature->name) + 1);

    if (key == kEventConfigUsedKey && output.used) {
      EventConfig event_config;
      if (!ParseEventConfig(param_value, &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_USED_EVENT_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.used = event_config;
    } else if (key == kEventConfigTriggerKey && output.trigger) {
      EventConfig event_config;
      if (!ParseEventConfig(param_value, &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.trigger = event_config;
    } else if (key == kSessionRateKey && output.session_rate) {
      Comparator comparator;
      if (!ParseComparator(param_value, &comparator)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.session_rate = comparator;
    } else if (key == kSessionRateImpactKey && output.session_rate_impact) {
      SessionRateImpact parsed_session_rate_impact;
      if (!ParseSessionRateImpact(param_value, &parsed_session_rate_impact,
                                  feature, all_features)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_IMPACT_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.session_rate_impact = parsed_session_rate_impact;
    } else if (key == kBlockingKey && output.blocking) {
      Blocking parsed_blocking;
      if (!ParseBlocking(param_value, &parsed_blocking)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_BLOCKING_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.blocking = parsed_blocking;
    } else if (key == kBlockedByKey && output.blocked_by) {
      BlockedBy parsed_blocked_by;
      if (!ParseBlockedBy(param_value, &parsed_blocked_by, feature,
                          all_features)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_BLOCKED_BY_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.blocked_by = parsed_blocked_by;
    } else if (key == kTrackingOnlyKey && output.tracking_only) {
      bool parsed_tracking_only;
      if (!ParseTrackingOnly(param_value, &parsed_tracking_only)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_TRACKING_ONLY_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.tracking_only = parsed_tracking_only;
    } else if (key == kAvailabilityKey && output.availability) {
      Comparator comparator;
      if (!ParseComparator(param_value, &comparator)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_AVAILABILITY_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.availability = comparator;
    } else if (key == kSnoozeParams && output.snooze_params) {
      SnoozeParams parsed_snooze_params;
      if (!ParseSnoozeParams(param_value, &parsed_snooze_params)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SNOOZE_PARAMS_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.snooze_params = parsed_snooze_params;
    } else if (key == kGroupsKey && output.groups) {
      if (!base::FeatureList::IsEnabled(kIPHGroups)) {
        continue;
      }
      std::vector<std::string> groups;
      if (!ParseGroups(param_value, &groups, feature, all_groups)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_GROUPS_PARSE);
        ++output.parse_errors;
        continue;
      }
      *output.groups = groups;
    } else if (base::StartsWith(key, kEventConfigKeyPrefix,
                                base::CompareCase::INSENSITIVE_ASCII) &&
               output.event_configs) {
      EventConfig event_config;
      if (!ParseEventConfig(param_value, &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_OTHER_EVENT_PARSE);
        ++output.parse_errors;
        continue;
      }
      output.event_configs->insert(event_config);
    } else if (base::StartsWith(key, kIgnoredKeyPrefix,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      // Intentionally ignoring parameter using registered ignored prefix.
      DVLOG(2) << "Ignoring unknown key when parsing config for feature "
               << feature->name << ": " << param_name;
    } else {
      DVLOG(1) << "Unknown key found when parsing config for feature "
               << feature->name << ": " << param_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::FAILURE_UNKNOWN_KEY);
    }
  }
}

template <typename T>
void RecordParseResult(std::string name, T config) {
  if (config.valid) {
    stats::RecordConfigParsingEvent(stats::ConfigParsingEvent::SUCCESS);
    DVLOG(2) << "Config for " << name << " is valid.";
    DVLOG(3) << "Config for " << name << " = " << config;
  } else {
    stats::RecordConfigParsingEvent(stats::ConfigParsingEvent::FAILURE);
    DVLOG(2) << "Config for " << name << " is invalid.";
  }
}

}  // namespace

ChromeVariationsConfiguration::ChromeVariationsConfiguration() = default;

ChromeVariationsConfiguration::~ChromeVariationsConfiguration() = default;

void ChromeVariationsConfiguration::ParseConfigs(const FeatureVector& features,
                                                 const GroupVector& groups) {
  for (auto* feature : features) {
    ParseFeatureConfig(feature, features, groups);
  }
  if (!base::FeatureList::IsEnabled(kIPHGroups)) {
    return;
  }

  for (auto* group : groups) {
    ParseGroupConfig(group, features, groups);
  }
}

bool ChromeVariationsConfiguration::ShouldUseClientSideConfig(
    const base::Feature* feature,
    base::FieldTrialParams* params) {
  // Client-side configuration is used under any of the following circumstances:
  // - The UseClientConfigIPH feature flag is enabled
  // - There are no field trial parameters set for the feature
  // - The field trial configuration is empty
  //
  // Note that the "empty configuration = use client-side" is quite useful, as
  // it means that json field trial configs, Finch configurations, and tests can
  // simply enable a feature engagement feature, and it will default to using
  // the client-side configuration; there is no need to duplicate a standard
  // configuration in more than one place.
  return base::FeatureList::IsEnabled(kUseClientConfigIPH) ||
         !base::GetFieldTrialParamsByFeature(*feature, params) ||
         params->empty();
}

void ChromeVariationsConfiguration::TryAddingClientSideConfig(
    const base::Feature* feature,
    bool is_group) {
  // Try to read the client-side configuration.
  bool added_client_config = (is_group)
                                 ? MaybeAddClientSideGroupConfig(feature)
                                 : MaybeAddClientSideFeatureConfig(feature);
  if (added_client_config) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::SUCCESS_FROM_SOURCE);
    DVLOG(3) << "Read checked in config for " << feature->name;
    return;
  }

  // No server-side nor client side configuration is available, but the
  // feature was on the list of available features, so give it an invalid
  // config.
  if (is_group) {
    GroupConfig& config = group_configs_[feature->name];
    config.valid = false;
  } else {
    FeatureConfig& config = configs_[feature->name];
    config.valid = false;
  }

  stats::RecordConfigParsingEvent(
      stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL);
  // Returns early. If no field trial, ConfigParsingEvent::FAILURE will not be
  // recorded.
  DVLOG(3) << "No field trial or checked in config for " << feature->name;
  return;
}

void ChromeVariationsConfiguration::ParseFeatureConfig(
    const base::Feature* feature,
    const FeatureVector& all_features,
    const GroupVector& all_groups) {
  DCHECK(feature);
  DCHECK(configs_.find(feature->name) == configs_.end());

  DVLOG(3) << "Parsing feature config for " << feature->name;
  std::map<std::string, std::string> params;
  if (ShouldUseClientSideConfig(feature, &params)) {
    TryAddingClientSideConfig(feature, /*is_group=*/false);
    return;
  }

  // Initially all new configurations are considered invalid.
  FeatureConfig& config = configs_[feature->name];
  config.valid = false;
  uint32_t parse_errors = 0;

  ConfigParseOutput output(parse_errors);
  output.session_rate = &config.session_rate;
  output.session_rate_impact = &config.session_rate_impact;
  output.blocking = &config.blocking;
  output.blocked_by = &config.blocked_by;
  output.trigger = &config.trigger;
  output.used = &config.used;
  output.event_configs = &config.event_configs;
  output.tracking_only = &config.tracking_only;
  output.availability = &config.availability;
  output.snooze_params = &config.snooze_params;
  output.groups = &config.groups;

  ParseConfigFields(feature, all_features, all_groups, params, output);

  // The |used| and |trigger| members are required, so should not be the
  // default values.
  bool has_used_event = config.used != EventConfig();
  bool has_trigger_event = config.trigger != EventConfig();
  config.valid = has_used_event && has_trigger_event && parse_errors == 0;

  RecordParseResult(feature->name, config);

  // Notice parse errors for used and trigger events will also cause the
  // following histograms being recorded.
  if (!has_used_event) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_USED_EVENT_MISSING);
  }
  if (!has_trigger_event) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING);
  }
}

bool ChromeVariationsConfiguration::MaybeAddClientSideFeatureConfig(
    const base::Feature* feature) {
  if (!base::FeatureList::IsEnabled(*feature))
    return false;

  DCHECK(configs_.find(feature->name) == configs_.end());
  if (auto config = GetClientSideFeatureConfig(feature)) {
    configs_[feature->name] = *config;
    return true;
  }
  return false;
}

void ChromeVariationsConfiguration::ParseGroupConfig(
    const base::Feature* group,
    const FeatureVector& all_features,
    const GroupVector& all_groups) {
  DCHECK(group);
  DCHECK(group_configs_.find(group->name) == group_configs_.end());

  DVLOG(3) << "Parsing group config for " << group->name;

  std::map<std::string, std::string> params;
  if (ShouldUseClientSideConfig(group, &params)) {
    TryAddingClientSideConfig(group, /*is_group=*/true);
    return;
  }

  // Initially all new configurations are considered invalid.
  GroupConfig& config = group_configs_[group->name];
  config.valid = false;
  uint32_t parse_errors = 0;

  ConfigParseOutput output(parse_errors);
  output.session_rate = &config.session_rate;
  output.trigger = &config.trigger;
  output.event_configs = &config.event_configs;

  ParseConfigFields(group, all_features, all_groups, params, output);

  // The |trigger| member is required, so should not be the
  // default value.
  bool has_trigger_event = config.trigger != EventConfig();
  config.valid = has_trigger_event && parse_errors == 0;

  RecordParseResult(group->name, config);

  // Notice parse errors for trigger event will also cause the
  // following histogram to be recorded.
  if (!has_trigger_event) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING);
  }
}

bool ChromeVariationsConfiguration::MaybeAddClientSideGroupConfig(
    const base::Feature* group) {
  if (!base::FeatureList::IsEnabled(*group)) {
    return false;
  }

  DCHECK(configs_.find(group->name) == configs_.end());
  if (auto config = GetClientSideGroupConfig(group)) {
    group_configs_[group->name] = *config;
    return true;
  }
  return false;
}

const FeatureConfig& ChromeVariationsConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  auto it = configs_.find(feature.name);
  DCHECK(it != configs_.end());
  return it->second;
}

const FeatureConfig& ChromeVariationsConfiguration::GetFeatureConfigByName(
    const std::string& feature_name) const {
  auto it = configs_.find(feature_name);
  DCHECK(it != configs_.end());
  return it->second;
}

const Configuration::ConfigMap&
ChromeVariationsConfiguration::GetRegisteredFeatureConfigs() const {
  return configs_;
}

const std::vector<std::string>
ChromeVariationsConfiguration::GetRegisteredFeatures() const {
  std::vector<std::string> features;
  for (const auto& element : configs_)
    features.push_back(element.first);
  return features;
}

const GroupConfig& ChromeVariationsConfiguration::GetGroupConfig(
    const base::Feature& group) const {
  auto it = group_configs_.find(group.name);
  DCHECK(it != group_configs_.end());
  return it->second;
}

const GroupConfig& ChromeVariationsConfiguration::GetGroupConfigByName(
    const std::string& group_name) const {
  auto it = group_configs_.find(group_name);
  DCHECK(it != group_configs_.end());
  return it->second;
}

const Configuration::GroupConfigMap&
ChromeVariationsConfiguration::GetRegisteredGroupConfigs() const {
  return group_configs_;
}

const std::vector<std::string>
ChromeVariationsConfiguration::GetRegisteredGroups() const {
  std::vector<std::string> groups;
  for (const auto& element : group_configs_)
    groups.push_back(element.first);
  return groups;
}

}  // namespace feature_engagement
