// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_configurations.h"
#include "components/feature_engagement/public/feature_list.h"

namespace {

const char kComparatorTypeAny[] = "any";
const char kComparatorTypeLessThan[] = "<";
const char kComparatorTypeGreaterThan[] = ">";
const char kComparatorTypeLessThanOrEqual[] = "<=";
const char kComparatorTypeGreaterThanOrEqual[] = ">=";
const char kComparatorTypeEqual[] = "==";
const char kComparatorTypeNotEqual[] = "!=";

const char kSessionRateImpactTypeAll[] = "all";
const char kSessionRateImpactTypeNone[] = "none";

const char kEventConfigUsedKey[] = "event_used";
const char kEventConfigTriggerKey[] = "event_trigger";
const char kEventConfigKeyPrefix[] = "event_";
const char kSessionRateKey[] = "session_rate";
const char kSessionRateImpactKey[] = "session_rate_impact";
const char kAvailabilityKey[] = "availability";
const char kTrackingOnlyKey[] = "tracking_only";
const char kIgnoredKeyPrefix[] = "x_";

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
  if (base::LowerCaseEqualsASCII(definition, kComparatorTypeAny)) {
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

    if (base::LowerCaseEqualsASCII(key, kEventConfigDataNameKey)) {
      if (has_name) {
        *event_config = EventConfig();
        return false;
      }
      has_name = true;

      event_config->name = value.as_string();
    } else if (base::LowerCaseEqualsASCII(key, kEventConfigDataComparatorKey)) {
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
    } else if (base::LowerCaseEqualsASCII(key, kEventConfigDataWindowKey)) {
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
    } else if (base::LowerCaseEqualsASCII(key, kEventConfigDataStorageKey)) {
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
    if (feature->name == feature_name.as_string())
      return true;
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

  if (base::LowerCaseEqualsASCII(trimmed_def, kSessionRateImpactTypeAll)) {
    session_rate_impact->type = SessionRateImpact::Type::ALL;
    return true;
  }

  if (base::LowerCaseEqualsASCII(trimmed_def, kSessionRateImpactTypeNone)) {
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
    if (base::LowerCaseEqualsASCII(feature_name, kSessionRateImpactTypeAll) ||
        base::LowerCaseEqualsASCII(feature_name, kSessionRateImpactTypeNone)) {
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
    affected_features.push_back(feature_name.as_string());
  }

  if (affected_features.empty())
    return false;

  session_rate_impact->type = SessionRateImpact::Type::EXPLICIT;
  session_rate_impact->affected_features = std::move(affected_features);
  return true;
}

bool ParseTrackingOnly(const base::StringPiece& definition,
                       bool* tracking_only) {
  // Since |tracking_only| is a primitive, ensure it set.
  *tracking_only = false;

  base::StringPiece trimmed_def =
      base::TrimWhitespaceASCII(definition, base::TRIM_ALL);

  if (base::LowerCaseEqualsASCII(trimmed_def, kTrackingOnlyTrue)) {
    *tracking_only = true;
    return true;
  }

  return base::LowerCaseEqualsASCII(trimmed_def, kTrackingOnlyFalse);
}
}  // namespace

ChromeVariationsConfiguration::ChromeVariationsConfiguration() = default;

ChromeVariationsConfiguration::~ChromeVariationsConfiguration() = default;

void ChromeVariationsConfiguration::ParseFeatureConfigs(
    const FeatureVector& features) {
  for (auto* feature : features) {
    ParseFeatureConfig(feature, features);
  }
}

void ChromeVariationsConfiguration::ParseFeatureConfig(
    const base::Feature* feature,
    const FeatureVector& all_features) {
  DCHECK(feature);
  DCHECK(configs_.find(feature->name) == configs_.end());

  DVLOG(3) << "Parsing feature config for " << feature->name;

  std::map<std::string, std::string> params;
  bool result = base::GetFieldTrialParamsByFeature(*feature, &params);
  // No |result| means that there was no server side configuration, or the
  // feature was disabled. The feature could be disabled either because it
  // is not configured to be base::FEATURE_ENABLED_BY_DEFAULT, or it has been
  // disabled from the server.
  if (!result) {
    // Some features have a checked in client side configuration, and for those
    // use that and and record success, otherwise fall back to invalid
    // configuration below.
    if (MaybeAddClientSideFeatureConfig(feature)) {
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::SUCCESS_FROM_SOURCE);
      DVLOG(3) << "Read checked in config for " << feature->name;
      return;
    }

    // No server-side, nor client side configuration available, but the feature
    // was passed in as one of all the feature available, so give it an invalid
    // config.
    FeatureConfig& config = configs_[feature->name];
    config.valid = false;

    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL);
    // Returns early. If no field trial, ConfigParsingEvent::FAILURE will not be
    // recorded.
    DVLOG(3) << "No field trial or checked in config for " << feature->name;
    return;
  }

  // Initially all new configurations are considered invalid.
  FeatureConfig& config = configs_[feature->name];
  config.valid = false;
  uint32_t parse_errors = 0;

  for (const auto& it : params) {
    const std::string& key = it.first;
    if (key == kEventConfigUsedKey) {
      EventConfig event_config;
      if (!ParseEventConfig(params[key], &event_config)) {
        ++parse_errors;
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_USED_EVENT_PARSE);
        continue;
      }
      config.used = event_config;
    } else if (key == kEventConfigTriggerKey) {
      EventConfig event_config;
      if (!ParseEventConfig(params[key], &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_PARSE);
        ++parse_errors;
        continue;
      }
      config.trigger = event_config;
    } else if (key == kSessionRateKey) {
      Comparator comparator;
      if (!ParseComparator(params[key], &comparator)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_PARSE);
        ++parse_errors;
        continue;
      }
      config.session_rate = comparator;
    } else if (key == kSessionRateImpactKey) {
      SessionRateImpact impact;
      if (!ParseSessionRateImpact(params[key], &impact, feature,
                                  all_features)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_IMPACT_PARSE);
        ++parse_errors;
        continue;
      }
      config.session_rate_impact = impact;
    } else if (key == kTrackingOnlyKey) {
      bool tracking_only;
      if (!ParseTrackingOnly(params[key], &tracking_only)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_TRACKING_ONLY_PARSE);
        ++parse_errors;
        continue;
      }
      config.tracking_only = tracking_only;
    } else if (key == kAvailabilityKey) {
      Comparator comparator;
      if (!ParseComparator(params[key], &comparator)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_AVAILABILITY_PARSE);
        ++parse_errors;
        continue;
      }
      config.availability = comparator;
    } else if (base::StartsWith(key, kEventConfigKeyPrefix,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      EventConfig event_config;
      if (!ParseEventConfig(params[key], &event_config)) {
        stats::RecordConfigParsingEvent(
            stats::ConfigParsingEvent::FAILURE_OTHER_EVENT_PARSE);
        ++parse_errors;
        continue;
      }
      config.event_configs.insert(event_config);
    } else if (base::StartsWith(key, kIgnoredKeyPrefix,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      // Intentionally ignoring parameter using registered ignored prefix.
      DVLOG(2) << "Ignoring unknown key when parsing config for feature "
               << feature->name << ": " << key;
    } else {
      DVLOG(1) << "Unknown key found when parsing config for feature "
               << feature->name << ": " << key;
      stats::RecordConfigParsingEvent(
          stats::ConfigParsingEvent::FAILURE_UNKNOWN_KEY);
    }
  }

  // The |used| and |trigger| members are required, so should not be the
  // default values.
  bool has_used_event = config.used != EventConfig();
  bool has_trigger_event = config.trigger != EventConfig();
  config.valid = has_used_event && has_trigger_event && parse_errors == 0;

  if (config.valid) {
    stats::RecordConfigParsingEvent(stats::ConfigParsingEvent::SUCCESS);
    DVLOG(2) << "Config for " << feature->name << " is valid.";
    DVLOG(3) << "Config for " << feature->name << " = " << config;
  } else {
    stats::RecordConfigParsingEvent(stats::ConfigParsingEvent::FAILURE);
    DVLOG(2) << "Config for " << feature->name << " is invalid.";
  }

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

}  // namespace feature_engagement
