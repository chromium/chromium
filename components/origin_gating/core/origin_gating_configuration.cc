// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"

namespace origin_gating {

namespace {

constexpr DecisionSource kForbiddenPredicates[] = {
    DecisionSource::kNoVerdict,
};

}  // namespace

CustomPredicate::CustomPredicate(AsyncPredicate predicate,
                                 std::string_view name)
    : predicate_(std::move(predicate)), name_(name) {}

CustomPredicate::CustomPredicate(SyncPredicate predicate, std::string_view name)
    : CustomPredicate(base::BindRepeating(
                          [](SyncPredicate sync_pred,
                             const GatingDecisionContext* context,
                             GateableEvent event,
                             const GURL& source,
                             const GURL& destination,
                             base::OnceCallback<void(Decision)> callback) {
                            std::move(callback).Run(sync_pred.Run(
                                context, event, source, destination));
                          },
                          std::move(predicate)),
                      name) {}

CustomPredicate::~CustomPredicate() = default;

CustomPredicate::CustomPredicate(const CustomPredicate&) = default;

CustomPredicate& CustomPredicate::operator=(const CustomPredicate&) = default;

void CustomPredicate::Run(const GatingDecisionContext* context,
                          GateableEvent event,
                          const GURL& source,
                          const GURL& destination,
                          base::OnceCallback<void(Decision)> callback) const {
  predicate_.Run(context, event, source, destination, std::move(callback));
}

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
}

OriginGatingConfiguration::~OriginGatingConfiguration() = default;

OriginGatingConfiguration::OriginGatingConfiguration(
    const OriginGatingConfiguration&) = default;

OriginGatingConfiguration& OriginGatingConfiguration::operator=(
    const OriginGatingConfiguration&) = default;

}  // namespace origin_gating
