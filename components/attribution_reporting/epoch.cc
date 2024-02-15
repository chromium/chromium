// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/epoch.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kEpochStart[] = "epoch_start";
constexpr char kEpochEnd[] = "epoch_end";

bool IsEpochValid(const uint64_t epoch_start, uint64_t epoch_end) {
  return epoch_start <= epoch_end && epoch_start >=0 && epoch_end >=0;
}

base::expected<uint64_t, TriggerRegistrationError> ParseEpochStart(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find(kEpochStart);
  if (!v) {
    return base::unexpected(
        TriggerRegistrationError::kEpochStartMissing);
  }
  if (std::optional<uint64_t> epoch_start = v->GetIfInt()) {
    if (*epoch_start < 0) {
      return base::unexpected(TriggerRegistrationError::kEpochValueInvalid);
    }
    return *epoch_start;
  }
    return base::unexpected(
        TriggerRegistrationError::kEpochStartMissing);
}

base::expected<uint64_t, TriggerRegistrationError> ParseEpochEnd(
    const base::Value::Dict& registration) {
  const base::Value* v = registration.Find(kEpochEnd);
  if (!v) {
    return base::unexpected(
        TriggerRegistrationError::kEpochEndMissing);
  }
  if (std::optional<uint64_t> epoch_end = v->GetIfInt()) {
    if (*epoch_end < 0) {
      return base::unexpected(TriggerRegistrationError::kEpochValueInvalid);
    }
    return *epoch_end;
  }
    return base::unexpected(
        TriggerRegistrationError::kEpochEndMissing);
}


}  // namespace

// static
std::optional<Epoch> Epoch::Create(uint64_t epoch_start, uint64_t epoch_end) {
  if (!IsEpochValid(epoch_start, epoch_end))
    return std::nullopt;
  return Epoch(epoch_start, epoch_end);
}

// static
base::expected<Epoch, TriggerRegistrationError>
Epoch::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kEpochWrongType);
  }

  ASSIGN_OR_RETURN(auto epoch_start, ParseEpochStart(*dict));
  ASSIGN_OR_RETURN(auto epoch_end, ParseEpochEnd(*dict));
  return Epoch(epoch_start, epoch_end);
}

Epoch::Epoch() = default;

Epoch::Epoch(uint64_t epoch_start, uint64_t epoch_end)
    : epoch_start_(epoch_start), epoch_end_(epoch_end) {
  DCHECK(IsEpochValid(epoch_start_, epoch_end_));
}

Epoch::~Epoch() = default;

Epoch::Epoch(
    const Epoch&) = default;

Epoch& Epoch::operator=(
    const Epoch&) = default;

Epoch::Epoch(Epoch&&) =
    default;

Epoch& Epoch::operator=(
    Epoch&&) = default;

base::Value::Dict Epoch::ToJson() const {
  base::Value::Dict dict;

  SerializeUint64(dict, kEpochStart, epoch_start_);
  SerializeUint64(dict, kEpochEnd, epoch_end_);
  return dict;
}

}  // namespace attribution_reporting
