// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/types.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

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
    GateableEvent event,
    const GURL& source,
    const GURL& destination,
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunNextPredicate(std::move(context), config_.predicates(),
                   DelegateInputs{
                       .event = event,
                       .source = source,
                       .source_origin = url::Origin::Create(source),
                       .destination = destination,
                       .destination_origin = url::Origin::Create(destination),
                       .requires_user_confirmation = std::nullopt,
                   },
                   std::move(callback));
}

void OriginGatingChecker::RunNextPredicate(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> pending_predicates,
    DelegateInputs input,
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Skip predicates that don't apply to the current event.
  while (!pending_predicates.empty() &&
         !pending_predicates.front().AppliesTo(input.event)) {
    pending_predicates = pending_predicates.subspan(1u);
  }

  GatingDecisionContext* raw_context = context.get();
  if (pending_predicates.empty()) {
    if (input.requires_user_confirmation.has_value()) {
      delegate_->OnNoVerdict(
          raw_context, input.event, input.source, input.destination,
          input.requires_user_confirmation.value(),
          base::BindOnce(&OriginGatingChecker::OnNoVerdictAnswer,
                         weak_ptr_factory_.GetWeakPtr(), std::move(context),
                         input.destination, std::move(callback)));
    } else {
      delegate_->DoesOriginRequireUserConfirmation(
          raw_context, input.event, input.source, input.destination,
          base::BindOnce(&OriginGatingChecker::OnUserConfirmationRequiredAnswer,
                         weak_ptr_factory_.GetWeakPtr(), std::move(context),
                         pending_predicates, input, std::move(callback)));
    }
    return;
  }

  const PredicateConfiguration::Predicate& predicate =
      pending_predicates.front().predicate();
  base::span<const PredicateConfiguration> remaining_predicates =
      pending_predicates.subspan(1u);

  std::visit(
      absl::Overload{
          [&](DecisionSource source_enum) {
            switch (source_enum) {
              case DecisionSource::kAllowSameOrigin: {
                Decision decision = EvaluateAllowSameOrigin(
                    input.source_origin, input.destination_origin);
                OnPredicateVerdict(std::move(context), remaining_predicates,
                                   DecisionAttribution(source_enum),
                                   std::move(input), std::move(callback),
                                   decision);
                break;
              }
              case DecisionSource::kCacheWithUserConfirmation: {
                Decision decision =
                    IsCachedWithUserConfirmation(input.destination_origin);
                OnPredicateVerdict(std::move(context), remaining_predicates,
                                   DecisionAttribution(source_enum),
                                   std::move(input), std::move(callback),
                                   decision);
                break;
              }
              case DecisionSource::kCacheWithoutUserConfirmation: {
                if (input.requires_user_confirmation.has_value()) {
                  Decision decision =
                      !input.requires_user_confirmation.value() &&
                              cache_.IsNavigationAllowed(
                                  input.source_origin, input.destination_origin)
                          ? Decision::kAllowed
                          : Decision::kNoDecision;
                  OnPredicateVerdict(std::move(context), remaining_predicates,
                                     DecisionAttribution(source_enum),
                                     std::move(input), std::move(callback),
                                     decision);
                } else {
                  GateableEvent event = input.event;
                  GURL source = input.source;
                  GURL destination = input.destination;
                  delegate_->DoesOriginRequireUserConfirmation(
                      raw_context, event, source, destination,
                      base::BindOnce(&OriginGatingChecker::
                                         OnUserConfirmationRequiredAnswer,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(context), pending_predicates,
                                     std::move(input), std::move(callback)));
                }
                break;
              }
              case DecisionSource::kNoVerdict:
                // These are internal/fallback decision sources and are not
                // executable predicates. OriginGatingConfiguration's
                // constructor guarantees that these are never present in the
                // predicates list, making this block unreachable.
                NOTREACHED();
            }
          },
          [&](const CustomPredicate& custom_predicate) {
            GatingDecisionContext* raw_context = context.get();
            custom_predicate.Run(
                raw_context, input.event, input.source, input.destination,
                base::BindOnce(&OriginGatingChecker::OnPredicateVerdict,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(context), remaining_predicates,
                               DecisionAttribution(custom_predicate.name()),
                               input, std::move(callback)));
          }},
      predicate);
}

void OriginGatingChecker::OnPredicateVerdict(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> remaining_predicates,
    DecisionAttribution attribution,
    DelegateInputs input,
    GatingDecisionCallback callback,
    Decision decision) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (decision) {
    case Decision::kAllowed:
      ResolveGatingDecision(std::move(callback), std::move(context),
                            GatingDecision{
                                .is_allowed = true,
                                .attribution = std::move(attribution),
                            });
      return;
    case Decision::kBlocked:
      ResolveGatingDecision(std::move(callback), std::move(context),
                            GatingDecision{
                                .is_allowed = false,
                                .attribution = std::move(attribution),
                            });
      return;
    case Decision::kNoDecision:
      RunNextPredicate(std::move(context), remaining_predicates,
                       std::move(input), std::move(callback));
      return;
  }
  NOTREACHED();
}

void OriginGatingChecker::OnUserConfirmationRequiredAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> pending_predicates,
    DelegateInputs input,
    GatingDecisionCallback callback,
    bool requires_user_confirmation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  input.requires_user_confirmation = requires_user_confirmation;
  RunNextPredicate(std::move(context), pending_predicates, std::move(input),
                   std::move(callback));
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

  ResolveGatingDecision(
      std::move(callback), std::move(context),
      GatingDecision{
          .is_allowed = result.is_allowed,
          .attribution = DecisionAttribution(DecisionSource::kNoVerdict),
      });
}

Decision OriginGatingChecker::IsCachedWithUserConfirmation(
    const url::Origin& origin) const {
  return cache_.IsNavigationConfirmedByUser(origin) ? Decision::kAllowed
                                                    : Decision::kNoDecision;
}

}  // namespace origin_gating
