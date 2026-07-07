// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/types.h"

#include <utility>

#include "base/check.h"

namespace origin_gating {

DecisionAttribution::DecisionAttribution(DecisionSource source)
    : attribution_(source) {}

DecisionAttribution::DecisionAttribution(std::string custom_predicate_name)
    : attribution_(std::move(custom_predicate_name)) {}

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

const std::string& DecisionAttribution::CustomPredicateName() const {
  CHECK(is_custom_predicate());
  return std::get<std::string>(attribution_);
}

bool DecisionAttribution::operator==(DecisionSource source) const {
  return is_source() && Source() == source;
}

bool DecisionAttribution::operator==(std::string_view name) const {
  return is_custom_predicate() && CustomPredicateName() == name;
}

bool DecisionAttribution::operator==(const DecisionAttribution& other) const =
    default;

}  // namespace origin_gating
