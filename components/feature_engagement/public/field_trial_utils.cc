// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/field_trial_utils.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/feature_engagement/public/group_list.h"
#include "components/feature_engagement/public/stats.h"

namespace feature_engagement {

namespace {

constexpr char kComparatorTypeAny[] = "any";
constexpr char kComparatorTypeLessThan[] = "<";
constexpr char kComparatorTypeGreaterThan[] = ">";
constexpr char kComparatorTypeLessThanOrEqual[] = "<=";
constexpr char kComparatorTypeGreaterThanOrEqual[] = ">=";
constexpr char kComparatorTypeEqual[] = "==";
constexpr char kComparatorTypeNotEqual[] = "!=";

constexpr char kEventConfigUsedKey[] = "event_used";
constexpr char kEventConfigTriggerKey[] = "event_trigger";
constexpr char kEventConfigKeyPrefix[] = "event_";
constexpr char kSessionRateKey[] = "session_rate";
constexpr char kSessionRateImpactKey[] = "session_rate_impact";
constexpr char kBlockingKey[] = "blocking";
constexpr char kBlockedByKey[] = "blocked_by";
constexpr char kAvailabilityKey[] = "availability";
constexpr char kTrackingOnlyKey[] = "tracking_only";
constexpr char kGroupsKey[] = "groups";
constexpr char kIgnoredKeyPrefix[] = "x_";

constexpr char kEventConfigDataNameKey[] = "name";
constexpr char kEventConfigDataComparatorKey[] = "comparator";
constexpr char kEventConfigDataWindowKey[] = "window";
constexpr char kEventConfigDataStorageKey[] = "storage";

constexpr char kImpactedFeaturesTypeAll[] = "all";
constexpr char kImpactedFeaturesTypeNone[] = "none";

constexpr char kSnoozeParams[] = "snooze_params";
constexpr char kSnoozeParamsMaxLimit[] = "max_limit";
constexpr char kSnoozeParamsInterval[] = "snooze_interval";

constexpr char kTrackingOnlyTrue[] = "true";
constexpr char kTrackingOnlyFalse[] = "false";

bool ParseComparatorSubstring(std::string_view definition,
                              Comparator* comparator,
                              ComparatorType type,
                              uint32_t type_len) {
  std::string_view number_string =
      base::TrimWhitespaceASCII(definition.substr(type_len), base::TRIM_ALL);
  uint32_t value;
  if (!base::StringToUint(number_string, &value)) {
    return false;
  }

  comparator->type = type;
  comparator->value = value;
  return true;
}

bool ParseComparator(std::string_view definition, Comparator* comparator) {
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

bool ParseEventConfig(std::string_view definition, EventConfig* event_config) {
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

    std::string_view key = pair[0];
    std::string_view value = pair[1];
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

bool ParseSessionRateImpact(std::string_view definition,
                            SessionRateImpact* session_rate_impact,
                            const base::Feature* this_feature,
                            const FeatureVector& known_features,
                            const GroupVector& known_groups) {
  std::string_view trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0) {
    return false;
  }

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
  if (parsed_feature_names.empty()) {
    return false;
  }

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
    if (!ContainsFeature(feature_name, known_features) &&
        !ContainsFeature(feature_name, known_groups)) {
      DVLOG(1) << "Unknown feature name found when parsing session_rate_impact "
               << "for feature " << this_feature->name << ": " << feature_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::
              FAILURE_SESSION_RATE_IMPACT_UNKNOWN_FEATURE);
      continue;
    }
    affected_features.emplace_back(feature_name);
  }

  if (affected_features.empty()) {
    return false;
  }

  session_rate_impact->type = SessionRateImpact::Type::EXPLICIT;
  session_rate_impact->affected_features = std::move(affected_features);
  return true;
}

bool ParseBlockedBy(std::string_view definition,
                    BlockedBy* blocked_by,
                    const base::Feature* this_feature,
                    const FeatureVector& known_features,
                    const GroupVector& known_groups) {
  std::string_view trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0) {
    return false;
  }

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
  if (parsed_feature_names.empty()) {
    return false;
  }

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
    if (!ContainsFeature(feature_name, known_features) &&
        !ContainsFeature(feature_name, known_groups)) {
      DVLOG(1) << "Unknown feature name found when parsing blocked_by "
               << "for feature " << this_feature->name << ": " << feature_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::FAILURE_BLOCKED_BY_UNKNOWN_FEATURE);
      continue;
    }
    affected_features.emplace_back(feature_name);
  }

  if (affected_features.empty()) {
    return false;
  }

  blocked_by->type = BlockedBy::Type::EXPLICIT;
  blocked_by->affected_features = std::move(affected_features);
  return true;
}

bool ParseBlocking(std::string_view definition, Blocking* blocking) {
  std::string_view trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (trimmed_def.length() == 0) {
    return false;
  }

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

bool ParseSnoozeParams(std::string_view definition,
                       SnoozeParams* snooze_params) {
  auto tokens = base::SplitStringPiece(definition, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
  if (tokens.size() != 2) {
    return false;
  }

  bool has_max_limit = false;
  bool has_snooze_interval = false;
  for (const auto& token : tokens) {
    auto pair = base::SplitStringPiece(token, ":", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);

    if (pair.size() != 2) {
      return false;
    }

    std::string_view key = pair[0];
    std::string_view value = pair[1];
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

bool ParseTrackingOnly(std::string_view definition, bool* tracking_only) {
  // Since |tracking_only| is a primitive, ensure it set.
  *tracking_only = false;

  std::string_view trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (base::EqualsCaseInsensitiveASCII(trimmed_def, kTrackingOnlyTrue)) {
    *tracking_only = true;
    return true;
  }

  return base::EqualsCaseInsensitiveASCII(trimmed_def, kTrackingOnlyFalse);
}

bool ParseGroups(std::string_view definition,
                 std::vector<std::string>* groups,
                 const base::Feature* this_feature,
                 const GroupVector& known_groups) {
  std::string_view trimmed_def =
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
      DVLOG(1) << "Empty group name when parsing groups " << "for feature "
               << this_feature->name;
      continue;
    }
    if (!ContainsFeature(group_name, known_groups)) {
      DVLOG(1) << "Unknown group name found when parsing groups "
               << "for feature " << this_feature->name << ": " << group_name;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::FAILURE_GROUPS_UNKNOWN_GROUP);
      continue;
    }
    groups->emplace_back(group_name);
  }

  if (groups->empty()) {
    return false;
  }

  return true;
}

}  // namespace

ConfigParseOutput::ConfigParseOutput(uint32_t& parse_errors)
    : parse_errors(parse_errors) {}

void ParseConfigFields(const base::Feature* feature,
                       std::map<std::string, std::string> params,
                       ConfigParseOutput& output,
                       const FeatureVector& known_features,
                       const GroupVector& known_groups) {
  for (const auto& it : params) {
    std::string param_name = it.first;
    std::string param_value = params[param_name];
    std::string key = param_name;
    // The param name might have a prefix containing the feature name with
    // a trailing underscore, e.g. IPH_FooFeature_session_rate. Strip out
    // the feature prefix for further comparison.
    if (base::StartsWith(key, feature->name, base::CompareCase::SENSITIVE)) {
      key = param_name.substr(strlen(feature->name) + 1);
    }

    if (key == kEventConfigUsedKey && output.used) {
      EventConfig event_config;
      if (!ParseEventConfig(param_value, &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_USED_EVENT_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.used = event_config;
    } else if (key == kEventConfigTriggerKey && output.trigger) {
      EventConfig event_config;
      if (!ParseEventConfig(param_value, &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.trigger = event_config;
    } else if (key == kSessionRateKey && output.session_rate) {
      Comparator comparator;
      if (!ParseComparator(param_value, &comparator)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.session_rate = comparator;
    } else if (key == kSessionRateImpactKey && output.session_rate_impact) {
      SessionRateImpact parsed_session_rate_impact;
      if (!ParseSessionRateImpact(param_value, &parsed_session_rate_impact,
                                  feature, known_features, known_groups)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_IMPACT_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.session_rate_impact = parsed_session_rate_impact;
    } else if (key == kBlockingKey && output.blocking) {
      Blocking parsed_blocking;
      if (!ParseBlocking(param_value, &parsed_blocking)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_BLOCKING_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.blocking = parsed_blocking;
    } else if (key == kBlockedByKey && output.blocked_by) {
      BlockedBy parsed_blocked_by;
      if (!ParseBlockedBy(param_value, &parsed_blocked_by, feature,
                          known_features, known_groups)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_BLOCKED_BY_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.blocked_by = parsed_blocked_by;
    } else if (key == kTrackingOnlyKey && output.tracking_only) {
      bool parsed_tracking_only;
      if (!ParseTrackingOnly(param_value, &parsed_tracking_only)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_TRACKING_ONLY_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.tracking_only = parsed_tracking_only;
    } else if (key == kAvailabilityKey && output.availability) {
      Comparator comparator;
      if (!ParseComparator(param_value, &comparator)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_AVAILABILITY_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.availability = comparator;
    } else if (key == kSnoozeParams && output.snooze_params) {
      SnoozeParams parsed_snooze_params;
      if (!ParseSnoozeParams(param_value, &parsed_snooze_params)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SNOOZE_PARAMS_PARSE);
        ++*output.parse_errors;
        continue;
      }
      *output.snooze_params = parsed_snooze_params;
    } else if (key == kGroupsKey && output.groups) {
      std::vector<std::string> groups;
      if (!ParseGroups(param_value, &groups, feature, known_groups)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_GROUPS_PARSE);
        ++*output.parse_errors;
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
        ++*output.parse_errors;
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

}  // namespace feature_engagement
