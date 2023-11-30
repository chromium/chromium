// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_level_epsilon.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

constexpr char kEventLevelEpsilon[] = "event_level_epsilon";

double g_max_event_level_epsilon = 14;

bool IsEventLevelEpsilonValid(double epsilon) {
  return epsilon >= 0 && epsilon <= g_max_event_level_epsilon;
}

}  // namespace

// static
base::expected<EventLevelEpsilon, SourceRegistrationError>
EventLevelEpsilon::Parse(const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kEventLevelEpsilon);
  if (!value) {
    return EventLevelEpsilon();
  }

  absl::optional<double> d = value->GetIfDouble();
  if (!d.has_value()) {
    return base::unexpected(
        SourceRegistrationError::kEventLevelEpsilonWrongType);
  }

  if (!IsEventLevelEpsilonValid(*d)) {
    return base::unexpected(
        SourceRegistrationError::kEventLevelEpsilonValueInvalid);
  }

  return EventLevelEpsilon(*d);
}

EventLevelEpsilon::EventLevelEpsilon()
    : EventLevelEpsilon(g_max_event_level_epsilon) {}

EventLevelEpsilon::EventLevelEpsilon(double epsilon) : epsilon_(epsilon) {
  CHECK(IsEventLevelEpsilonValid(epsilon_));
}

bool EventLevelEpsilon::SetIfValid(double epsilon) {
  if (!IsEventLevelEpsilonValid(epsilon)) {
    return false;
  }
  epsilon_ = epsilon;
  return true;
}

void EventLevelEpsilon::Serialize(base::Value::Dict& dict) const {
  dict.Set(kEventLevelEpsilon, epsilon_);
}

ScopedMaxEventLevelEpsilonForTesting::ScopedMaxEventLevelEpsilonForTesting(
    double epsilon)
    : previous_(g_max_event_level_epsilon) {
  CHECK_GE(epsilon, 0);
  g_max_event_level_epsilon = epsilon;
}

ScopedMaxEventLevelEpsilonForTesting::~ScopedMaxEventLevelEpsilonForTesting() {
  g_max_event_level_epsilon = previous_;
}

}  // namespace attribution_reporting
