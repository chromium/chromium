// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_DATA_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AttributionScopesData {
 public:
  static std::optional<AttributionScopesData> Create(
      AttributionScopesSet,
      std::optional<uint32_t> attribution_scope_limit,
      uint32_t max_event_states);

  static base::expected<AttributionScopesData, mojom::SourceRegistrationError>
  FromJSON(base::Value::Dict&);

  AttributionScopesData();
  ~AttributionScopesData();

  AttributionScopesData(const AttributionScopesData&);
  AttributionScopesData(AttributionScopesData&&);

  AttributionScopesData& operator=(const AttributionScopesData&);
  AttributionScopesData& operator=(AttributionScopesData&&);

  const AttributionScopesSet& attribution_scopes_set() const {
    return attribution_scopes_set_;
  }

  std::optional<uint32_t> attribution_scope_limit() const {
    return attribution_scope_limit_;
  }

  uint32_t max_event_states() const { return max_event_states_; }

  void Serialize(base::Value::Dict&) const;

  friend bool operator==(const AttributionScopesData&,
                         const AttributionScopesData&) = default;

 private:
  AttributionScopesData(AttributionScopesSet,
                        std::optional<uint32_t> attribution_scope_limit,
                        uint32_t max_event_states);

  AttributionScopesSet attribution_scopes_set_;
  std::optional<uint32_t> attribution_scope_limit_;
  uint32_t max_event_states_ = kDefaultMaxEventStates;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_DATA_H_
