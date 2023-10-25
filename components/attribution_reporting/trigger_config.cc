// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <string>

#include "base/feature_list.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerDataMatching;

constexpr char kTriggerDataMatching[] = "trigger_data_matching";

constexpr char kTriggerDataMatchingExact[] = "exact";
constexpr char kTriggerDataMatchingModulus[] = "modulus";

base::expected<TriggerDataMatching, SourceRegistrationError>
ParseTriggerDataMatching(const base::Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return base::unexpected(
        SourceRegistrationError::kTriggerDataMatchingWrongType);
  } else if (*str == kTriggerDataMatchingExact) {
    return TriggerDataMatching::kExact;
  } else if (*str == kTriggerDataMatchingModulus) {
    return TriggerDataMatching::kModulus;
  } else {
    return base::unexpected(
        SourceRegistrationError::kTriggerDataMatchingUnknownValue);
  }
}

std::string SerializeTriggerDataMatching(TriggerDataMatching v) {
  switch (v) {
    case TriggerDataMatching::kExact:
      return kTriggerDataMatchingExact;
    case TriggerDataMatching::kModulus:
      return kTriggerDataMatchingModulus;
  }
}

void SerializeTriggerConfig(const TriggerConfig& config,
                            base::Value::Dict& dict) {
  dict.Set(kTriggerDataMatching,
           SerializeTriggerDataMatching(config.trigger_data_matching()));
}

}  // namespace

TriggerConfig::TriggerConfig() = default;

TriggerConfig::TriggerConfig(TriggerDataMatching trigger_data_matching)
    : trigger_data_matching_(trigger_data_matching) {}

TriggerConfig::~TriggerConfig() = default;

TriggerConfig::TriggerConfig(const TriggerConfig&) = default;

TriggerConfig& TriggerConfig::operator=(const TriggerConfig&) = default;

TriggerConfig::TriggerConfig(TriggerConfig&&) = default;

TriggerConfig& TriggerConfig::operator=(TriggerConfig&&) = default;

// static
base::expected<TriggerConfig, SourceRegistrationError> TriggerConfig::Parse(
    const base::Value::Dict& dict) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionReportingTriggerConfig)) {
    return TriggerConfig();
  }

  TriggerConfig config;
  if (const base::Value* value = dict.Find(kTriggerDataMatching)) {
    ASSIGN_OR_RETURN(config.trigger_data_matching_,
                     ParseTriggerDataMatching(*value));
  }

  return config;
}

void TriggerConfig::Serialize(base::Value::Dict& dict) const {
  if (base::FeatureList::IsEnabled(
          features::kAttributionReportingTriggerConfig)) {
    SerializeTriggerConfig(*this, dict);
  }
}

void TriggerConfig::SerializeForTesting(base::Value::Dict& dict) const {
  SerializeTriggerConfig(*this, dict);
}

}  // namespace attribution_reporting
