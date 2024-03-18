// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_EVENT_LEVEL_EPSILON_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_EVENT_LEVEL_EPSILON_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"

namespace attribution_reporting {

// Controls the epsilon parameter used for obtaining a randomized response for
// the containing source registration.
//
// https://wicg.github.io/attribution-reporting-api/#obtain-a-randomized-source-response
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) EventLevelEpsilon {
 public:
  static base::expected<EventLevelEpsilon, mojom::SourceRegistrationError>
  Parse(const base::Value::Dict&);

  static double max();

  // Creates an epsilon with the maximum allowed value.
  EventLevelEpsilon();

  // `CHECK()`s that the given value is non-negative and less than the maximum.
  explicit EventLevelEpsilon(double);

  ~EventLevelEpsilon() = default;

  EventLevelEpsilon(const EventLevelEpsilon&) = default;
  EventLevelEpsilon& operator=(const EventLevelEpsilon&) = default;

  EventLevelEpsilon(EventLevelEpsilon&&) = default;
  EventLevelEpsilon& operator=(EventLevelEpsilon&&) = default;

  // This implicit conversion is allowed to ease drop-in use of
  // this type in places currently requiring `int` with prior validation.
  operator double() const {  // NOLINT
    return epsilon_;
  }

  [[nodiscard]] bool SetIfValid(double);

  void Serialize(base::Value::Dict&) const;

 private:
  double epsilon_;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    ScopedMaxEventLevelEpsilonForTesting {
 public:
  explicit ScopedMaxEventLevelEpsilonForTesting(double);

  ~ScopedMaxEventLevelEpsilonForTesting();

  ScopedMaxEventLevelEpsilonForTesting(
      const ScopedMaxEventLevelEpsilonForTesting&) = delete;
  ScopedMaxEventLevelEpsilonForTesting& operator=(
      const ScopedMaxEventLevelEpsilonForTesting&) = delete;

  ScopedMaxEventLevelEpsilonForTesting(ScopedMaxEventLevelEpsilonForTesting&&) =
      delete;
  ScopedMaxEventLevelEpsilonForTesting& operator=(
      ScopedMaxEventLevelEpsilonForTesting&&) = delete;

 private:
  double previous_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_EVENT_LEVEL_EPSILON_H_
