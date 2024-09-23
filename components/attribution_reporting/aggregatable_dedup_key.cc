// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_dedup_key.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

}  // namespace

// static
base::expected<AggregatableDedupKey, TriggerRegistrationError>
AggregatableDedupKey::FromJSON(base::Value& value) {
  base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableDedupKeyWrongType);
  }

  AggregatableDedupKey out;

  ASSIGN_OR_RETURN(out.dedup_key, ParseDeduplicationKey(*dict), [](ParseError) {
    return TriggerRegistrationError::kAggregatableDedupKeyValueInvalid;
  });

  ASSIGN_OR_RETURN(out.filters, FilterPair::FromJSON(*dict));

  return out;
}

AggregatableDedupKey::AggregatableDedupKey() = default;

AggregatableDedupKey::AggregatableDedupKey(std::optional<uint64_t> dedup_key,
                                           FilterPair filters)
    : dedup_key(dedup_key), filters(std::move(filters)) {}

base::Value::Dict AggregatableDedupKey::ToJson() const {
  base::Value::Dict dict;

  filters.SerializeIfNotEmpty(dict);

  SerializeDeduplicationKey(dict, dedup_key);

  return dict;
}

}  // namespace attribution_reporting
