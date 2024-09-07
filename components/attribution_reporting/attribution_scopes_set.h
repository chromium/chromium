// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_SET_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_SET_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AttributionScopesSet {
 public:
  using Scopes = base::flat_set<std::string>;

  static base::expected<AttributionScopesSet, mojom::SourceRegistrationError>
  FromJSON(base::Value::Dict&, uint32_t attribution_scope_limit);

  static base::expected<AttributionScopesSet, mojom::TriggerRegistrationError>
  FromJSON(base::Value::Dict&);

  explicit AttributionScopesSet(Scopes);

  AttributionScopesSet();
  ~AttributionScopesSet();

  AttributionScopesSet(const AttributionScopesSet&);
  AttributionScopesSet(AttributionScopesSet&&);

  AttributionScopesSet& operator=(const AttributionScopesSet&);
  AttributionScopesSet& operator=(AttributionScopesSet&&);

  const Scopes& scopes() const { return scopes_; }

  Scopes TakeScopes() && { return std::move(scopes_); }

  void SerializeForSource(base::Value::Dict&) const;

  void SerializeForTrigger(base::Value::Dict&) const;

  bool HasIntersection(const AttributionScopesSet& other_scopes) const;

  bool IsValidForSource(uint32_t scope_limit) const;

  friend bool operator==(const AttributionScopesSet&,
                         const AttributionScopesSet&) = default;

 private:
  Scopes scopes_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SCOPES_SET_H_
