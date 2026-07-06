// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/types.h"

namespace origin_gating {

namespace {

void PostTask(base::OnceClosure closure) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

void ResolveGatingDecision(GatingDecisionCallback callback,
                           std::unique_ptr<GatingDecisionContext> context,
                           GatingDecision decision) {
  PostTask(base::BindOnce(std::move(callback), std::move(context),
                          std::move(decision)));
}

Decision EvaluateAllowSameOrigin(const url::Origin& source,
                                 const url::Origin& destination) {
  return source.IsSameOriginWith(destination) ? Decision::kAllowed
                                              : Decision::kNoDecision;
}

}  // namespace

OriginGatingChecker::OriginGatingChecker(Delegate& delegate,
                                         OriginGatingConfiguration config)
    : delegate_(delegate),
      config_(std::move(config)),
      cache_(config_.use_site_keyed_cache()) {}

OriginGatingChecker::~OriginGatingChecker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OriginGatingChecker::ComputeGatingDecision(
    std::unique_ptr<GatingDecisionContext> context,
    const GURL& source,
    const GURL& destination,
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunNextPredicate(
      std::move(context), config_.predicates(),
      DelegateInputs{source, url::Origin::Create(source), destination,
                     url::Origin::Create(destination)},
      std::move(callback));
}

void OriginGatingChecker::RunNextPredicate(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const DecisionSource> pending_predicates,
    DelegateInputs input,
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_predicates.empty()) {
    GatingDecisionContext* raw_context = context.get();
    delegate_->DoesOriginRequireUserConfirmation(
        raw_context, input.source, input.destination,
        base::BindOnce(&OriginGatingChecker::OnUserConfirmationRequiredAnswer,
                       weak_ptr_factory_.GetWeakPtr(), std::move(context),
                       input, std::move(callback)));
    return;
  }

  DecisionSource source_enum = pending_predicates.front();
  base::span<const DecisionSource> remaining_predicates =
      pending_predicates.subspan(1u);

  switch (source_enum) {
    case DecisionSource::kAllowSameOrigin: {
      OnPredicateVerdict(std::move(context), remaining_predicates, source_enum,
                         input, std::move(callback),
                         EvaluateAllowSameOrigin(input.source_origin,
                                                 input.destination_origin));
      break;
    }
    case DecisionSource::kCacheWithUserConfirmation:
      OnPredicateVerdict(
          std::move(context), remaining_predicates, source_enum, input,
          std::move(callback),
          IsCachedWithUserConfirmation(input.destination_origin));
      break;
    case DecisionSource::kCache:
    case DecisionSource::kNoVerdict:
      // These are internal/fallback decision sources and are not executable
      // predicates. OriginGatingConfiguration's constructor guarantees that
      // these are never present in the predicates list, making this block
      // unreachable.
      NOTREACHED();
  }
}

void OriginGatingChecker::OnPredicateVerdict(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const DecisionSource> remaining_predicates,
    DecisionSource attribution,
    DelegateInputs input,
    GatingDecisionCallback callback,
    Decision decision) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (decision) {
    case Decision::kAllowed:
      ResolveGatingDecision(std::move(callback), std::move(context),
                            GatingDecision{
                                .is_allowed = true,
                                .source = attribution,
                            });
      return;
    case Decision::kBlocked:
      ResolveGatingDecision(std::move(callback), std::move(context),
                            GatingDecision{
                                .is_allowed = false,
                                .source = attribution,
                            });
      return;
    case Decision::kNoDecision:
      RunNextPredicate(std::move(context), remaining_predicates, input,
                       std::move(callback));
      return;
  }
  NOTREACHED();
}

void OriginGatingChecker::OnUserConfirmationRequiredAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    DelegateInputs input,
    GatingDecisionCallback callback,
    bool requires_user_confirmation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!requires_user_confirmation &&
      cache_.IsNavigationAllowed(input.source_origin,
                                 input.destination_origin)) {
    ResolveGatingDecision(std::move(callback), std::move(context),
                          GatingDecision{
                              .is_allowed = true,
                              .source = DecisionSource::kCache,
                          });
    return;
  }

  GatingDecisionContext* raw_context = context.get();
  delegate_->OnNoVerdict(
      raw_context, input.source, input.destination, requires_user_confirmation,
      base::BindOnce(&OriginGatingChecker::OnNoVerdictAnswer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     input.destination, std::move(callback)));
}

void OriginGatingChecker::OnNoVerdictAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    const GURL& destination,
    GatingDecisionCallback callback,
    Delegate::NoVerdictResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.is_allowed) {
    AllowNavigationTo(url::Origin::Create(destination), result.did_prompt_user);
  }

  ResolveGatingDecision(std::move(callback), std::move(context),
                        GatingDecision{
                            .is_allowed = result.is_allowed,
                            .source = DecisionSource::kNoVerdict,
                        });
}

Decision OriginGatingChecker::IsCachedWithUserConfirmation(
    const url::Origin& origin) const {
  return cache_.IsNavigationConfirmedByUser(origin) ? Decision::kAllowed
                                                    : Decision::kNoDecision;
}

}  // namespace origin_gating
