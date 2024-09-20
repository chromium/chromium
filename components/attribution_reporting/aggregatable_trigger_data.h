// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableTriggerData {
 public:
  using Keys = base::flat_set<std::string>;

  static base::expected<AggregatableTriggerData,
                        mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  AggregatableTriggerData();

  AggregatableTriggerData(absl::uint128 key_piece,
                          Keys source_keys,
                          FilterPair);

  ~AggregatableTriggerData();

  AggregatableTriggerData(const AggregatableTriggerData&);
  AggregatableTriggerData& operator=(const AggregatableTriggerData&);

  AggregatableTriggerData(AggregatableTriggerData&&);
  AggregatableTriggerData& operator=(AggregatableTriggerData&&);

  absl::uint128 key_piece() const { return key_piece_; }

  const Keys& source_keys() const { return source_keys_; }

  const FilterPair& filters() const { return filters_; }

  base::Value::Dict ToJson() const;

  friend bool operator==(const AggregatableTriggerData&,
                         const AggregatableTriggerData&) = default;

 private:
  absl::uint128 key_piece_ = 0;
  Keys source_keys_;
  FilterPair filters_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_
