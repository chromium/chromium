// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CHECKER_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CHECKER_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {

class OriginGatingChecker {
 public:
  // Pure virtual interface for embedder-specific checks.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    struct NoVerdictResult {
      bool is_allowed;
      bool did_prompt_user;
      // When true, the checker does not persist this allow decision to the
      // cache. Delegates set this for events whose result should not be
      // remembered (e.g. a provisional navigation request/redirect
      // destination). Ignored when `is_allowed` is false.
      bool bypass_cache = false;
    };

    using DoesOriginRequireUserConfirmationCallback =
        base::OnceCallback<void(bool)>;
    // Evaluates whether the given destination URL requires confirmation from
    // the user when navigating from the source URL. Invokes the callback with
    // the result.
    virtual void DoesOriginRequireUserConfirmation(
        GatingDecisionContext* context,
        GateableEvent event,
        const GURL& source,
        const GURL& destination,
        DoesOriginRequireUserConfirmationCallback callback) const = 0;

    // Defers the final decision from the OriginGatingChecker to the delegate.
    virtual void OnNoVerdict(
        GatingDecisionContext* context,
        GateableEvent event,
        const GURL& source,
        const GURL& destination,
        bool requires_user_confirmation,
        base::OnceCallback<void(NoVerdictResult)> callback) = 0;
  };

  // The delegate must outlive this OriginGatingChecker instance.
  OriginGatingChecker(Delegate& delegate, OriginGatingConfiguration config);
  ~OriginGatingChecker();

  OriginGatingChecker(const OriginGatingChecker&) = delete;
  OriginGatingChecker& operator=(const OriginGatingChecker&) = delete;

  // Evaluates a navigation/actuation. `event` selects which predicates apply.
  // The callback is guaranteed to be invoked asynchronously on the same
  // sequence.
  void ComputeGatingDecision(std::unique_ptr<GatingDecisionContext> context,
                             GateableEvent event,
                             const GURL& source,
                             const GURL& destination,
                             GatingDecisionCallback callback);

  // Exposes mutation methods to manage allowed origins in the cache.
  void AllowNavigationTo(url::Origin origin, bool is_user_confirmed) {
    cache_.AllowNavigationTo(std::move(origin), is_user_confirmed);
  }
  void AllowNavigationTo(const absl::flat_hash_set<url::Origin>& origins) {
    cache_.AllowNavigationTo(origins);
  }

  const OriginGatingCache& cache() const { return cache_; }

 private:
  // Holds various inputs provided by the delegate (and data derived thereof),
  // to avoid needless recomputations.
  struct DelegateInputs {
    GateableEvent event;
    GURL source;
    url::Origin source_origin;
    GURL destination;
    url::Origin destination_origin;
    std::optional<bool> requires_user_confirmation;
  };

  void RunNextPredicate(
      std::unique_ptr<GatingDecisionContext> context,
      base::span<const PredicateConfiguration> pending_predicates,
      DelegateInputs input,
      GatingDecisionCallback callback);

  void OnPredicateVerdict(
      std::unique_ptr<GatingDecisionContext> context,
      base::span<const PredicateConfiguration> remaining_predicates,
      DecisionAttribution attribution,
      DelegateInputs input,
      GatingDecisionCallback callback,
      Decision decision);

  void OnUserConfirmationRequiredAnswer(
      std::unique_ptr<GatingDecisionContext> context,
      base::span<const PredicateConfiguration> pending_predicates,
      DelegateInputs input,
      GatingDecisionCallback callback,
      bool requires_user_confirmation);

  void OnNoVerdictAnswer(std::unique_ptr<GatingDecisionContext> context,
                         const GURL& destination,
                         GatingDecisionCallback callback,
                         Delegate::NoVerdictResult result);

  // Predicate that returns `kAllowed` if `destination` is in the cache with
  // user confirmation; `kNoDecision` otherwise.
  Decision IsCachedWithUserConfirmation(const url::Origin& origin) const;

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ref<Delegate> delegate_;
  OriginGatingConfiguration config_;
  OriginGatingCache cache_;
  base::WeakPtrFactory<OriginGatingChecker> weak_ptr_factory_{this};
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CHECKER_H_
