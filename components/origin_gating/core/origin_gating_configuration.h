// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_

#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/origin_gating/core/concepts.h"
#include "components/origin_gating/core/decision.h"
#include "components/origin_gating/core/decision_attribution.h"
#include "components/origin_gating/core/decision_source.h"
#include "components/origin_gating/core/gateable_event.h"
#include "components/origin_gating/core/gating_decision.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "url/gurl.h"

namespace origin_gating {

// Represents a custom, ID-tagged check provided by the embedder.
class CustomPredicate {
 public:
  using AsyncPredicate = base::RepeatingCallback<void(
      GatingDecisionContext* context,
      const GateableEvent& event,
      base::OnceCallback<void(Decision)> callback)>;

  using SyncPredicate =
      base::RepeatingCallback<Decision(GatingDecisionContext* context,
                                       const GateableEvent& event)>;

  // Constructs a CustomPredicate tagged with the given enum as its source.
  template <IsIntCompatibleEnum E>
  CustomPredicate(AsyncPredicate predicate, E id)
      : CustomPredicate(std::move(predicate),
                        DecisionAttribution::CustomPredicateAttribution(id)) {}

  // Constructs a CustomPredicate tagged with the given enum as its source.
  template <IsIntCompatibleEnum E>
  CustomPredicate(SyncPredicate predicate, E id)
      : CustomPredicate(std::move(predicate),
                        DecisionAttribution::CustomPredicateAttribution(id)) {}

  ~CustomPredicate();

  CustomPredicate(const CustomPredicate&);
  CustomPredicate& operator=(const CustomPredicate&);

  const std::variant<AsyncPredicate, SyncPredicate>& predicate() const {
    return predicate_;
  }

  const DecisionAttribution::CustomPredicateAttribution& attribution() const {
    return attribution_;
  }

 private:
  CustomPredicate(
      AsyncPredicate predicate,
      const DecisionAttribution::CustomPredicateAttribution& attribution);
  CustomPredicate(
      SyncPredicate predicate,
      const DecisionAttribution::CustomPredicateAttribution& attribution);

  std::variant<AsyncPredicate, SyncPredicate> predicate_;
  DecisionAttribution::CustomPredicateAttribution attribution_;
};

// Pairs a predicate with the set of gateable events it applies to. When
// ComputeGatingDecision is invoked for a given event, only predicates whose
// `applicable_events` contains that event are executed, the rest are skipped.
class PredicateConfiguration {
 public:
  using Predicate = std::variant<DecisionSource, CustomPredicate>;

  // Pairs a predicate with the set of gateable events it applies to.
  PredicateConfiguration(Predicate predicate,
                         GateableEventSet applicable_events);
  ~PredicateConfiguration();

  PredicateConfiguration(const PredicateConfiguration&);
  PredicateConfiguration& operator=(const PredicateConfiguration&);

  const Predicate& predicate() const { return predicate_; }

  // Returns whether this predicate should be evaluated for `event_type`.
  bool AppliesTo(GateableEvent::Type event_type) const;

 private:
  Predicate predicate_;
  GateableEventSet applicable_events_;
};

class OriginGatingConfiguration {
 public:
  using CacheScope = OriginGatingCache::CacheScope;

  // `predicates` specifies the ordered sequence of decision predicates to
  // execute. Each entry is a PredicateConfiguration that restricts the
  // predicate to specific events. All CustomPredicate entries must have been
  // created using the same enum domain.
  //
  // The following internal/fallback states are strictly forbidden:
  // - `DecisionSource::kNoVerdict`
  //
  // The following predicates require a navigation source URL and must not be
  // configured to apply to `GateableEvent::kPageAction`:
  // - `DecisionSource::kAllowSameOrigin`
  //
  // Note: `cache_scope` is ignored if `predicates` does not include a cache
  // predicate (`kCacheWithUserConfirmation` or
  // `kCacheWithoutUserConfirmation`).
  OriginGatingConfiguration(std::vector<PredicateConfiguration> predicates,
                            CacheScope cache_scope);
  ~OriginGatingConfiguration();

  OriginGatingConfiguration(const OriginGatingConfiguration&);
  OriginGatingConfiguration& operator=(const OriginGatingConfiguration&);

  const std::vector<PredicateConfiguration>& predicates() const {
    return predicates_;
  }

  std::optional<CacheScope> cache_scope() const { return cache_scope_; }

 private:
  std::vector<PredicateConfiguration> predicates_;
  std::optional<CacheScope> cache_scope_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
