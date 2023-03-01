// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableTriggerData {
 public:
  using Keys = std::vector<std::string>;

  static absl::optional<AggregatableTriggerData> Create(absl::uint128 key_piece,
                                                        Keys source_keys,
                                                        FilterPair);

  static base::expected<AggregatableTriggerData,
                        mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  AggregatableTriggerData();

  ~AggregatableTriggerData();

  AggregatableTriggerData(const AggregatableTriggerData&);
  AggregatableTriggerData& operator=(const AggregatableTriggerData&);

  AggregatableTriggerData(AggregatableTriggerData&&);
  AggregatableTriggerData& operator=(AggregatableTriggerData&&);

  absl::uint128 key_piece() const { return key_piece_; }

  const Keys& source_keys() const { return source_keys_; }

  const FilterPair& filters() const { return filters_; }

  base::Value::Dict ToJson() const;

 private:
  AggregatableTriggerData(absl::uint128 key_piece,
                          Keys source_keys,
                          FilterPair);

  absl::uint128 key_piece_ = 0;
  Keys source_keys_;
  FilterPair filters_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_
