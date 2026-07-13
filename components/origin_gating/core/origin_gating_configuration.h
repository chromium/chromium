// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_

#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "components/origin_gating/core/types.h"
#include "url/origin.h"

namespace origin_gating {

// Represents a custom, name-tagged check provided by the embedder.
class CustomPredicate {
 public:
  using AsyncPredicate = base::RepeatingCallback<void(
      const GatingDecisionContext* context,
      GateableEvent event,
      const GURL& source,
      const GURL& destination,
      base::OnceCallback<void(Decision)> callback)>;

  using SyncPredicate =
      base::RepeatingCallback<Decision(const GatingDecisionContext* context,
                                       GateableEvent event,
                                       const GURL& source,
                                       const GURL& destination)>;

  CustomPredicate(AsyncPredicate predicate, std::string_view name);
  CustomPredicate(SyncPredicate predicate, std::string_view name);
  ~CustomPredicate();

  CustomPredicate(const CustomPredicate&);
  CustomPredicate& operator=(const CustomPredicate&);

  void Run(const GatingDecisionContext* context,
           GateableEvent event,
           const GURL& source,
           const GURL& destination,
           base::OnceCallback<void(Decision)> callback) const;

  const std::string& name() const { return name_; }

 private:
  AsyncPredicate predicate_;
  std::string name_;
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

  // Returns whether this predicate should be evaluated for `event`.
  bool AppliesTo(GateableEvent event) const;

 private:
  Predicate predicate_;
  GateableEventSet applicable_events_;
};

class OriginGatingConfiguration {
 public:
  // `predicates` specifies the ordered sequence of decision predicates to
  // execute. Each entry is a PredicateConfiguration that restricts the
  // predicate to specific events.
  //
  // The following internal/fallback states are strictly forbidden:
  // - `DecisionSource::kNoVerdict`
  OriginGatingConfiguration(
      std::initializer_list<PredicateConfiguration> predicates,
      bool use_site_keyed_cache);
  ~OriginGatingConfiguration();

  OriginGatingConfiguration(const OriginGatingConfiguration&);
  OriginGatingConfiguration& operator=(const OriginGatingConfiguration&);

  const std::vector<PredicateConfiguration>& predicates() const {
    return predicates_;
  }
  bool use_site_keyed_cache() const { return use_site_keyed_cache_; }

 private:
  std::vector<PredicateConfiguration> predicates_;
  bool use_site_keyed_cache_ = false;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
