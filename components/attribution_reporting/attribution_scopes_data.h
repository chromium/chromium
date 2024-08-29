// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_DATA_H_

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AttributionScopesData {
 public:
  static std::optional<AttributionScopesData> Create(
      AttributionScopesSet,
      uint32_t attribution_scope_limit,
      uint32_t max_event_states);

  static base::expected<AttributionScopesData, mojom::SourceRegistrationError>
  FromJSON(base::Value&);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit AttributionScopesData(mojo::DefaultConstruct::Tag);

  ~AttributionScopesData();

  AttributionScopesData(const AttributionScopesData&);
  AttributionScopesData(AttributionScopesData&&);

  AttributionScopesData& operator=(const AttributionScopesData&);
  AttributionScopesData& operator=(AttributionScopesData&&);

  const AttributionScopesSet& attribution_scopes_set() const {
    return attribution_scopes_set_;
  }

  AttributionScopesSet TakeAttributionScopesSet() && {
    return std::move(attribution_scopes_set_);
  }

  uint32_t attribution_scope_limit() const { return attribution_scope_limit_; }

  uint32_t max_event_states() const { return max_event_states_; }

  base::Value::Dict ToJson() const;

  friend bool operator==(const AttributionScopesData&,
                         const AttributionScopesData&) = default;

 private:
  AttributionScopesData(AttributionScopesSet,
                        uint32_t attribution_scope_limit,
                        uint32_t max_event_states);

  AttributionScopesSet attribution_scopes_set_;
  uint32_t attribution_scope_limit_ = 0;
  uint32_t max_event_states_ = 0;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_DATA_H_
