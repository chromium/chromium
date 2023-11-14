// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATION_KEYS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATION_KEYS_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregationKeys {
 public:
  using Keys = base::flat_map<std::string, absl::uint128>;

  // Returns `absl::nullopt` if `keys` is invalid.
  static absl::optional<AggregationKeys> FromKeys(Keys keys);

  static base::expected<AggregationKeys, mojom::SourceRegistrationError>
  FromJSON(const base::Value*);

  AggregationKeys();
  ~AggregationKeys();

  AggregationKeys(const AggregationKeys&);
  AggregationKeys(AggregationKeys&&);

  AggregationKeys& operator=(const AggregationKeys&);
  AggregationKeys& operator=(AggregationKeys&&);

  const Keys& keys() const { return keys_; }

  base::Value::Dict ToJson() const;

  friend bool operator==(const AggregationKeys&,
                         const AggregationKeys&) = default;

 private:
  explicit AggregationKeys(Keys keys);

  Keys keys_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATION_KEYS_H_
