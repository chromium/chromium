// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_trigger_config.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::TriggerRegistrationError;

base::expected<mojom::SourceRegistrationTimeConfig, TriggerRegistrationError>
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

bool IsValid(SourceRegistrationTimeConfig source_registration_time_config,
             const std::optional<std::string>& trigger_context_id) {
  if (!trigger_context_id.has_value()) {
    return true;
  }

  return IsTriggerContextIdValid(*trigger_context_id) &&
         IsTriggerContextIdAllowed(source_registration_time_config);
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

  return AggregatableTriggerConfig(source_registration_time_config,
                                   std::move(trigger_context_id));
}

// static
std::optional<AggregatableTriggerConfig> AggregatableTriggerConfig::Create(
    SourceRegistrationTimeConfig source_registration_time_config,
    std::optional<std::string> trigger_context_id) {
  if (!IsValid(source_registration_time_config, trigger_context_id)) {
    return std::nullopt;
  }
  return AggregatableTriggerConfig(source_registration_time_config,
                                   std::move(trigger_context_id));
}

AggregatableTriggerConfig::AggregatableTriggerConfig() = default;

AggregatableTriggerConfig::AggregatableTriggerConfig(
    SourceRegistrationTimeConfig source_registration_time_config,
    std::optional<std::string> trigger_context_id)
    : source_registration_time_config_(source_registration_time_config),
      trigger_context_id_(std::move(trigger_context_id)) {
  CHECK(IsValid(source_registration_time_config_, trigger_context_id_));
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
}

}  // namespace attribution_reporting
