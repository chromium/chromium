// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <variant>

#include "base/check.h"
#include "components/origin_gating/core/types.h"

namespace origin_gating {

namespace {

constexpr DecisionSource kForbiddenPredicates[] = {
    DecisionSource::kNoVerdict,
};

bool UsesAtMostOneCustomPredicateDomain(
    base::span<const PredicateConfiguration> predicates) {
  auto first_custom_it =
      std::ranges::find_if(predicates, [](const PredicateConfiguration& pc) {
        return std::holds_alternative<CustomPredicate>(pc.predicate());
      });
  if (first_custom_it == predicates.end()) {
    return true;
  }
  const DecisionAttribution::CustomPredicateAttribution&
      first_custom_attribution =
          std::get<CustomPredicate>(first_custom_it->predicate()).attribution();
  return std::ranges::all_of(
      first_custom_it, predicates.end(), [&](const PredicateConfiguration& pc) {
        const auto* cp = std::get_if<CustomPredicate>(&pc.predicate());
        return !cp || cp->attribution().IsSameDomain(first_custom_attribution);
      });
}

}  // namespace

CustomPredicate::CustomPredicate(
    AsyncPredicate predicate,
    const DecisionAttribution::CustomPredicateAttribution& attribution)
    : predicate_(std::move(predicate)), attribution_(attribution) {}

CustomPredicate::CustomPredicate(
    SyncPredicate predicate,
    const DecisionAttribution::CustomPredicateAttribution& attribution)
    : predicate_(std::move(predicate)), attribution_(attribution) {}

CustomPredicate::~CustomPredicate() = default;

CustomPredicate::CustomPredicate(const CustomPredicate&) = default;

CustomPredicate& CustomPredicate::operator=(const CustomPredicate&) = default;

PredicateConfiguration::PredicateConfiguration(
    Predicate predicate,
    GateableEventSet applicable_events)
    : predicate_(std::move(predicate)), applicable_events_(applicable_events) {}

PredicateConfiguration::~PredicateConfiguration() = default;

bool PredicateConfiguration::AppliesTo(GateableEvent event) const {
  return applicable_events_.Has(event);
}

PredicateConfiguration::PredicateConfiguration(const PredicateConfiguration&) =
    default;

PredicateConfiguration& PredicateConfiguration::operator=(
    const PredicateConfiguration&) = default;

OriginGatingConfiguration::OriginGatingConfiguration(
    std::initializer_list<PredicateConfiguration> predicates,
    bool use_site_keyed_cache)
    : predicates_(predicates), use_site_keyed_cache_(use_site_keyed_cache) {
  CHECK(std::ranges::none_of(predicates, [](const PredicateConfiguration& pc) {
    const DecisionSource* source = std::get_if<DecisionSource>(&pc.predicate());
    return source && std::ranges::contains(kForbiddenPredicates, *source);
  }));
  CHECK(UsesAtMostOneCustomPredicateDomain(predicates_));
}

OriginGatingConfiguration::~OriginGatingConfiguration() = default;

OriginGatingConfiguration::OriginGatingConfiguration(
    const OriginGatingConfiguration&) = default;

OriginGatingConfiguration& OriginGatingConfiguration::operator=(
    const OriginGatingConfiguration&) = default;

}  // namespace origin_gating
