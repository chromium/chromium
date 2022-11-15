// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/bounded_list.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableTriggerData {
 public:
  static absl::optional<AggregatableTriggerData> Create(
      absl::uint128 key_piece,
      base::flat_set<std::string> source_keys,
      Filters filters,
      Filters not_filters);

  static base::expected<AggregatableTriggerData,
                        mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  ~AggregatableTriggerData();

  AggregatableTriggerData(const AggregatableTriggerData&);
  AggregatableTriggerData& operator=(const AggregatableTriggerData&);

  AggregatableTriggerData(AggregatableTriggerData&&);
  AggregatableTriggerData& operator=(AggregatableTriggerData&&);

  absl::uint128 key_piece() const { return key_piece_; }

  const base::flat_set<std::string>& source_keys() const {
    return source_keys_;
  }

  const Filters& filters() const { return filters_; }

  const Filters& not_filters() const { return not_filters_; }

 private:
  AggregatableTriggerData(absl::uint128 key_piece,
                          base::flat_set<std::string> source_keys,
                          Filters filters,
                          Filters not_filters);

  absl::uint128 key_piece_;
  base::flat_set<std::string> source_keys_;
  Filters filters_;
  Filters not_filters_;
};

using AggregatableTriggerDataList =
    BoundedList<AggregatableTriggerData, kMaxAggregatableTriggerDataPerTrigger>;

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_TRIGGER_DATA_H_
