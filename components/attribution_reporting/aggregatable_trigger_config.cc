// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_config.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::TriggerRegistrationError;

bool FilteringIdEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAttributionReportingAggregatableFilteringIds);
}

base::expected<SourceRegistrationTimeConfig, TriggerRegistrationError>
ParseAggregatableSourceRegistrationTime(const base::Value* value) {
  if (!value) {
    return SourceRegistrationTimeConfig::kExclude;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        TriggerRegistrationError::
            kAggregatableSourceRegistrationTimeValueInvalid);
  }

  if (*str == kSourceRegistrationTimeInclude) {
    return SourceRegistrationTimeConfig::kInclude;
  }

  if (*str == kSourceRegistrationTimeExclude) {
    return SourceRegistrationTimeConfig::kExclude;
  }

  return base::unexpected(TriggerRegistrationError::
                              kAggregatableSourceRegistrationTimeValueInvalid);
}

std::string SerializeAggregatableSourceRegistrationTime(
    SourceRegistrationTimeConfig config) {
  switch (config) {
    case SourceRegistrationTimeConfig::kInclude:
      return kSourceRegistrationTimeInclude;
    case SourceRegistrationTimeConfig::kExclude:
      return kSourceRegistrationTimeExclude;
  }
}

bool IsTriggerContextIdValid(const std::string& s) {
  return s.length() <= kMaxTriggerContextIdLength;
}

bool IsTriggerContextIdAllowed(
    SourceRegistrationTimeConfig source_registration_time_config) {
  switch (source_registration_time_config) {
    case SourceRegistrationTimeConfig::kExclude:
      return true;
    case SourceRegistrationTimeConfig::kInclude:
      return false;
  }
}

bool IsMaxBytesAllowed(
    AggregatableFilteringIdsMaxBytes max_bytes,
    SourceRegistrationTimeConfig source_registration_time_config) {
  switch (source_registration_time_config) {
    case SourceRegistrationTimeConfig::kExclude:
      return true;
    case SourceRegistrationTimeConfig::kInclude:
      return max_bytes.IsDefault();
  }
  NOTREACHED();
}

bool IsValid(SourceRegistrationTimeConfig source_registration_time_config,
             const std::optional<std::string>& trigger_context_id,
             AggregatableFilteringIdsMaxBytes max_bytes) {
  const bool trigger_context_id_valid =
      !trigger_context_id.has_value() ||
      (IsTriggerContextIdValid(*trigger_context_id) &&
       IsTriggerContextIdAllowed(source_registration_time_config));

  return trigger_context_id_valid &&
         IsMaxBytesAllowed(max_bytes, source_registration_time_config);
}

base::expected<std::optional<std::string>, TriggerRegistrationError>
ParseTriggerContextId(base::Value* value) {
  if (!value) {
    return std::nullopt;
  }

  std::string* s = value->GetIfString();
  if (!s || !IsTriggerContextIdValid(*s)) {
    return base::unexpected(
        TriggerRegistrationError::kTriggerContextIdInvalidValue);
  }
  return std::move(*s);
}

}  // namespace

// static
base::expected<AggregatableTriggerConfig, TriggerRegistrationError>
AggregatableTriggerConfig::Parse(base::Value::Dict& dict) {
  ASSIGN_OR_RETURN(SourceRegistrationTimeConfig source_registration_time_config,
                   ParseAggregatableSourceRegistrationTime(
                       dict.Find(kAggregatableSourceRegistrationTime)));

  ASSIGN_OR_RETURN(std::optional<std::string> trigger_context_id,
                   ParseTriggerContextId(dict.Find(kTriggerContextId)));
  if (trigger_context_id.has_value() &&
      !IsTriggerContextIdAllowed(source_registration_time_config)) {
    return base::unexpected(
        TriggerRegistrationError::
            kTriggerContextIdInvalidSourceRegistrationTimeConfig);
  }

  AggregatableFilteringIdsMaxBytes max_bytes;
  if (FilteringIdEnabled()) {
    ASSIGN_OR_RETURN(max_bytes, AggregatableFilteringIdsMaxBytes::Parse(dict));
    if (!IsMaxBytesAllowed(max_bytes, source_registration_time_config)) {
      return base::unexpected(
          TriggerRegistrationError::
              kAggregatableFilteringIdsMaxBytesInvalidSourceRegistrationTimeConfig);
    }
  }

  return AggregatableTriggerConfig(source_registration_time_config,
                                   std::move(trigger_context_id), max_bytes);
}

// static
std::optional<AggregatableTriggerConfig> AggregatableTriggerConfig::Create(
    SourceRegistrationTimeConfig source_registration_time_config,
    std::optional<std::string> trigger_context_id,
    AggregatableFilteringIdsMaxBytes max_bytes) {
  if (!IsValid(source_registration_time_config, trigger_context_id,
               max_bytes)) {
    return std::nullopt;
  }
  return AggregatableTriggerConfig(source_registration_time_config,
                                   std::move(trigger_context_id), max_bytes);
}

AggregatableTriggerConfig::AggregatableTriggerConfig() = default;

AggregatableTriggerConfig::AggregatableTriggerConfig(
    SourceRegistrationTimeConfig source_registration_time_config,
    std::optional<std::string> trigger_context_id,
    AggregatableFilteringIdsMaxBytes max_bytes)
    : source_registration_time_config_(source_registration_time_config),
      trigger_context_id_(std::move(trigger_context_id)),
      aggregatable_filtering_id_max_bytes_(max_bytes) {
  CHECK(IsValid(source_registration_time_config_, trigger_context_id_,
                aggregatable_filtering_id_max_bytes_));
}

AggregatableTriggerConfig::AggregatableTriggerConfig(
    const AggregatableTriggerConfig&) = default;

AggregatableTriggerConfig& AggregatableTriggerConfig::operator=(
    const AggregatableTriggerConfig&) = default;

AggregatableTriggerConfig::AggregatableTriggerConfig(
    AggregatableTriggerConfig&&) = default;

AggregatableTriggerConfig& AggregatableTriggerConfig::operator=(
    AggregatableTriggerConfig&&) = default;

AggregatableTriggerConfig::~AggregatableTriggerConfig() = default;

void AggregatableTriggerConfig::Serialize(base::Value::Dict& dict) const {
  dict.Set(kAggregatableSourceRegistrationTime,
           SerializeAggregatableSourceRegistrationTime(
               source_registration_time_config_));
  if (trigger_context_id_.has_value()) {
    dict.Set(kTriggerContextId, *trigger_context_id_);
  }
  if (FilteringIdEnabled()) {
    aggregatable_filtering_id_max_bytes_.Serialize(dict);
  }
}

bool AggregatableTriggerConfig::ShouldCauseAReportToBeSentUnconditionally()
    const {
  return trigger_context_id_.has_value() ||
         !aggregatable_filtering_id_max_bytes_.IsDefault();
}

}  // namespace attribution_reporting
