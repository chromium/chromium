// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <optional>

#include "base/check.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

constexpr uint8_t kDefaultMaxBytes = 1;

bool IsMaxBytesValueValid(int max_bytes_value) {
  static constexpr uint8_t kMaxMaxBytes = 8;
  return max_bytes_value > 0 && max_bytes_value <= kMaxMaxBytes;
}

}  // namespace

AggregatableFilteringIdsMaxBytes::AggregatableFilteringIdsMaxBytes()
    : AggregatableFilteringIdsMaxBytes(kDefaultMaxBytes) {}

AggregatableFilteringIdsMaxBytes::AggregatableFilteringIdsMaxBytes(int size)
    : value_(size) {
  CHECK(IsMaxBytesValueValid(size));
}

// static
std::optional<AggregatableFilteringIdsMaxBytes>
AggregatableFilteringIdsMaxBytes::Create(int size) {
  if (!IsMaxBytesValueValid(size)) {
    return std::nullopt;
  }
  return AggregatableFilteringIdsMaxBytes(size);
}

// static
base::expected<AggregatableFilteringIdsMaxBytes,
               mojom::TriggerRegistrationError>
AggregatableFilteringIdsMaxBytes::Parse(const base::Value::Dict& registration) {
  const base::Value* value =
      registration.Find(kAggregatableFilteringIdsMaxBytes);
  if (!value) {
    return AggregatableFilteringIdsMaxBytes();
  }

  ASSIGN_OR_RETURN(int v, ParseInt(*value), [](ParseError) {
    return mojom::TriggerRegistrationError::
        kAggregatableFilteringIdMaxBytesInvalidValue;
  });

  auto max_bytes = AggregatableFilteringIdsMaxBytes::Create(v);
  if (!max_bytes.has_value()) {
    return base::unexpected(mojom::TriggerRegistrationError::
                                kAggregatableFilteringIdMaxBytesInvalidValue);
  }

  return max_bytes.value();
}

void AggregatableFilteringIdsMaxBytes::Serialize(
    base::Value::Dict& dict) const {
  dict.Set(kAggregatableFilteringIdsMaxBytes, static_cast<int>(value_));
}

bool AggregatableFilteringIdsMaxBytes::IsDefault() const {
  return value_ == kDefaultMaxBytes;
}

bool AggregatableFilteringIdsMaxBytes::CanEncompass(
    uint64_t filtering_id) const {
  static constexpr size_t kBitsPerByte = 8;
  return static_cast<size_t>(std::bit_width(filtering_id)) <=
         (kBitsPerByte * value_);
}

}  // namespace attribution_reporting
