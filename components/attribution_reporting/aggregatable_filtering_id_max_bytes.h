// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_FILTERING_ID_MAX_BYTES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_FILTERING_ID_MAX_BYTES_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableFilteringIdsMaxBytes {
 public:
  static base::expected<AggregatableFilteringIdsMaxBytes,
                        mojom::TriggerRegistrationError>
  Parse(const base::Value::Dict&);

  static std::optional<AggregatableFilteringIdsMaxBytes> Create(int);

  // `CHECK()`s that the given value is positive and no greater than the
  // maximum.
  explicit AggregatableFilteringIdsMaxBytes(int);

  // Creates with the default size.
  AggregatableFilteringIdsMaxBytes();

  ~AggregatableFilteringIdsMaxBytes() = default;

  AggregatableFilteringIdsMaxBytes(const AggregatableFilteringIdsMaxBytes&) =
      default;
  AggregatableFilteringIdsMaxBytes& operator=(
      const AggregatableFilteringIdsMaxBytes&) = default;

  AggregatableFilteringIdsMaxBytes(AggregatableFilteringIdsMaxBytes&&) =
      default;
  AggregatableFilteringIdsMaxBytes& operator=(
      AggregatableFilteringIdsMaxBytes&&) = default;

  friend bool operator==(AggregatableFilteringIdsMaxBytes,
                         AggregatableFilteringIdsMaxBytes) = default;

  void Serialize(base::Value::Dict&) const;

  uint8_t value() const { return value_; }

  bool IsDefault() const;

  bool CanEncompass(uint64_t filtering_id) const;

 private:
  uint8_t value_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_FILTERING_ID_MAX_BYTES_H_
