// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/decision_attribution.h"

#include <variant>

#include "base/check.h"
#include "components/origin_gating/core/decision_source.h"

namespace origin_gating {

DecisionAttribution::DecisionAttribution(DecisionSource source)
    : attribution_(source) {}

DecisionAttribution::DecisionAttribution(
    const CustomPredicateAttribution& attribution)
    : attribution_(attribution) {}

DecisionAttribution::~DecisionAttribution() = default;

DecisionAttribution::DecisionAttribution(const DecisionAttribution&) = default;

DecisionAttribution& DecisionAttribution::operator=(
    const DecisionAttribution&) = default;

DecisionAttribution::DecisionAttribution(DecisionAttribution&&) = default;

DecisionAttribution& DecisionAttribution::operator=(DecisionAttribution&&) =
    default;

DecisionAttribution::Type DecisionAttribution::type() const {
  return std::holds_alternative<DecisionSource>(attribution_)
             ? Type::kDecisionSource
             : Type::kCustomPredicate;
}

DecisionSource DecisionAttribution::Source() const {
  CHECK(is_source());
  return std::get<DecisionSource>(attribution_);
}

bool DecisionAttribution::operator==(DecisionSource source) const {
  return is_source() && Source() == source;
}

}  // namespace origin_gating
