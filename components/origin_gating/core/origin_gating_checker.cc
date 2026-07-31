// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/types.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

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

Decision EvaluateAllowHttpLocalhost(const GURL& destination) {
  return net::IsLocalhost(destination) && destination.SchemeIsHTTPOrHTTPS()
             ? Decision::kAllowed
             : Decision::kNoDecision;
}

Decision EvaluateAllowAboutBlank(const GURL& destination) {
  return destination.IsAboutBlank() ? Decision::kAllowed
                                    : Decision::kNoDecision;
}

Decision EvaluateForbidIpAddress(const GURL& destination) {
  return destination.HostIsIPAddress() ? Decision::kBlocked
                                       : Decision::kNoDecision;
}

Decision EvaluateRequireHttps(const GURL& destination) {
  return destination.SchemeIs(url::kHttpsScheme) ? Decision::kNoDecision
                                                 : Decision::kBlocked;
}

Decision EvaluateRequireHttpsOrHttp(const GURL& destination) {
  return destination.SchemeIsHTTPOrHTTPS() ? Decision::kNoDecision
                                           : Decision::kBlocked;
}

// Returns true if the verdict was final and `context` and `callback` were
// consumed; false otherwise.
bool ProcessDecision(std::unique_ptr<GatingDecisionContext>& context,
                     DecisionAttribution attribution,
                     Decision decision,
                     GatingDecisionCallback& callback) {
  switch (decision) {
    case Decision::kAllowed:
      ResolveGatingDecision(std::move(callback), std::move(context),
                            GatingDecision{
                                .is_allowed = true,
                                .attribution = std::move(attribution),
                            });
      return true;
    case Decision::kBlocked:
      ResolveGatingDecision(std::move(callback), std::move(context),
                            GatingDecision{
                                .is_allowed = false,
                                .attribution = std::move(attribution),
                            });
      return true;
    case Decision::kNoDecision:
      return false;
  }
  NOTREACHED();
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
  EvaluatePredicates(std::move(context), config_.predicates(),
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

void OriginGatingChecker::EvaluatePredicates(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> pending_predicates,
    DelegateInputs input,
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < pending_predicates.size(); ++i) {
    const PredicateConfiguration& predicate_config = pending_predicates[i];
    if (!predicate_config.AppliesTo(input.event)) {
      continue;
    }

    bool consumed = std::visit(
        absl::Overload{
            [&](DecisionSource source_enum) VALID_CONTEXT_REQUIRED(
                sequence_checker_) {
              Decision decision;
              switch (source_enum) {
                case DecisionSource::kAllowSameOrigin: {
                  decision = EvaluateAllowSameOrigin(input.source_origin,
                                                     input.destination_origin);
                  break;
                }
                case DecisionSource::kAllowHttpLocalhost: {
                  decision = EvaluateAllowHttpLocalhost(input.destination);
                  break;
                }
                case DecisionSource::kAllowAboutBlank: {
                  decision = EvaluateAllowAboutBlank(input.destination);
                  break;
                }
                case DecisionSource::kCacheWithUserConfirmation: {
                  decision =
                      IsCachedWithUserConfirmation(input.destination_origin);
                  break;
                }
                case DecisionSource::kCacheWithoutUserConfirmation: {
                  return RunActionOrGetUserConfirmationInfo(
                      context, pending_predicates.subspan(/*offset=*/i), input,
                      callback,
                      [&](std::unique_ptr<GatingDecisionContext>& context,
                          DelegateInputs& input,
                          GatingDecisionCallback& callback)
                          VALID_CONTEXT_REQUIRED(sequence_checker_) {
                            Decision decision =
                                !input.requires_user_confirmation.value() &&
                                        cache_.IsNavigationAllowed(
                                            input.source_origin,
                                            input.destination_origin)
                                    ? Decision::kAllowed
                                    : Decision::kNoDecision;
                            return ProcessDecision(
                                context, DecisionAttribution(source_enum),
                                decision, callback);
                          });
                }
                case DecisionSource::kEnterprisePolicy: {
                  GURL destination = input.destination;
                  delegate_->EvaluateEnterprisePolicy(
                      destination,
                      base::BindOnce(
                          &OriginGatingChecker::OnEnterprisePolicyVerdict,
                          weak_ptr_factory_.GetWeakPtr(), std::move(context),
                          pending_predicates.subspan(/*offset=*/i + 1),
                          DecisionAttribution(source_enum), std::move(input),
                          std::move(callback)));
                  return true;
                }
                case DecisionSource::kForbidIpAddress: {
                  decision = EvaluateForbidIpAddress(input.destination);
                  break;
                }
                case DecisionSource::kRequireHttps: {
                  decision = EvaluateRequireHttps(input.destination);
                  break;
                }
                case DecisionSource::kRequireHttpsOrHttp: {
                  decision = EvaluateRequireHttpsOrHttp(input.destination);
                  break;
                }
                case DecisionSource::kActorContainerConfig: {
                  decision = EvaluateActorContainerConfig(
                      input.event, input.source_origin,
                      input.destination_origin);
                  break;
                }
                case DecisionSource::kNoVerdict:
                  // This is an internal/fallback decision source and is not an
                  // executable predicate. OriginGatingConfiguration's
                  // constructor guarantees that this is never present in the
                  // predicates list, making this block unreachable.
                  NOTREACHED();
              }
              return ProcessDecision(context, DecisionAttribution(source_enum),
                                     decision, callback);
            },
            [&](const CustomPredicate& custom_predicate)
                VALID_CONTEXT_REQUIRED(sequence_checker_) {
                  GatingDecisionContext* raw_context = context.get();
                  DecisionAttribution attribution(custom_predicate.name());
                  return std::visit(
                      absl::Overload{
                          [&](const CustomPredicate::AsyncPredicate& predicate)
                              VALID_CONTEXT_REQUIRED(sequence_checker_) {
                                GURL source = input.source;
                                GURL destination = input.destination;
                                predicate.Run(
                                    raw_context, source, destination,
                                    base::BindOnce(
                                        &OriginGatingChecker::
                                            OnEvaluatedAsyncPredicate,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(context),
                                        pending_predicates.subspan(
                                            /*offset=*/i + 1),
                                        std::move(attribution),
                                        std::move(input), std::move(callback)));
                                return true;
                              },
                          [&](const CustomPredicate::SyncPredicate& predicate) {
                            return ProcessDecision(
                                context, std::move(attribution),
                                predicate.Run(raw_context, input.source,
                                              input.destination),
                                callback);
                          },
                      },
                      custom_predicate.predicate());
                }},
        predicate_config.predicate());
    if (consumed) {
      return;
    }
  }

  RunActionOrGetUserConfirmationInfo(
      context, /*pending_predicates=*/{}, input, callback,
      [&](std::unique_ptr<GatingDecisionContext>& context,
          DelegateInputs& input, GatingDecisionCallback& callback)
          VALID_CONTEXT_REQUIRED(sequence_checker_) {
            GatingDecisionContext* raw_context = context.get();
            GateableEvent event = input.event;
            GURL source = input.source;
            GURL destination = input.destination;
            bool requires_user_confirmation =
                input.requires_user_confirmation.value();
            delegate_->OnNoVerdict(
                raw_context, event, source, destination,
                requires_user_confirmation,
                base::BindOnce(&OriginGatingChecker::OnNoVerdictAnswer,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(context), std::move(input),
                               std::move(callback)));
            return true;
          });
}

void OriginGatingChecker::OnEvaluatedAsyncPredicate(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> pending_predicates,
    DecisionAttribution attribution,
    DelegateInputs input,
    GatingDecisionCallback callback,
    Decision decision) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ProcessDecision(context, attribution, decision, callback)) {
    return;
  }

  EvaluatePredicates(std::move(context), pending_predicates, std::move(input),
                     std::move(callback));
}

void OriginGatingChecker::OnEnterprisePolicyVerdict(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> pending_predicates,
    DecisionAttribution attribution,
    DelegateInputs input,
    GatingDecisionCallback callback,
    Delegate::DecisionWithMetadata verdict) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (verdict.decision == Decision::kAllowed && !verdict.bypass_cache) {
    AllowNavigationTo(input.destination_origin, /*is_user_confirmed=*/false);
  }
  OnEvaluatedAsyncPredicate(std::move(context), pending_predicates,
                            std::move(attribution), std::move(input),
                            std::move(callback), verdict.decision);
}

void OriginGatingChecker::OnUserConfirmationRequiredAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    base::span<const PredicateConfiguration> pending_predicates,
    DelegateInputs input,
    GatingDecisionCallback callback,
    bool requires_user_confirmation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  input.requires_user_confirmation = requires_user_confirmation;
  EvaluatePredicates(std::move(context), pending_predicates, std::move(input),
                     std::move(callback));
}

void OriginGatingChecker::OnNoVerdictAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    DelegateInputs input,
    GatingDecisionCallback callback,
    Delegate::NoVerdictResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.is_allowed && !result.bypass_cache) {
    AllowNavigationTo(input.destination_origin, result.did_prompt_user);
  }

  ResolveGatingDecision(
      std::move(callback), std::move(context),
      GatingDecision{
          .is_allowed = result.is_allowed,
          .attribution = DecisionAttribution(DecisionSource::kNoVerdict),
      });
}

bool OriginGatingChecker::RunActionOrGetUserConfirmationInfo(
    std::unique_ptr<GatingDecisionContext>& context,
    base::span<const PredicateConfiguration> pending_predicates,
    DelegateInputs& input,
    GatingDecisionCallback& callback,
    base::FunctionRef<bool(std::unique_ptr<GatingDecisionContext>& context,
                           DelegateInputs& input,
                           GatingDecisionCallback& callback)> action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (input.requires_user_confirmation.has_value()) {
    return action(context, input, callback);
  }
  GatingDecisionContext* raw_context = context.get();
  GateableEvent event = input.event;
  GURL source = input.source;
  GURL destination = input.destination;
  delegate_->DoesOriginRequireUserConfirmation(
      raw_context, event, source, destination,
      base::BindOnce(&OriginGatingChecker::OnUserConfirmationRequiredAnswer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     pending_predicates, std::move(input),
                     std::move(callback)));
  return true;
}

Decision OriginGatingChecker::IsCachedWithUserConfirmation(
    const url::Origin& origin) const {
  return cache_.IsNavigationConfirmedByUser(origin) ? Decision::kAllowed
                                                    : Decision::kNoDecision;
}

Decision OriginGatingChecker::EvaluateActorContainerConfig(
    GateableEvent event,
    const url::Origin& source,
    const url::Origin& destination) const {
  if (!actor_container_config_slot_.has_value()) {
    return Decision::kNoDecision;
  }
  const ActorContainerConfig& config = actor_container_config_slot_.value();
  if (event == GateableEvent::kPageAction) {
    return config.IsActuationAllowed(destination) ? Decision::kAllowed
                                                  : Decision::kBlocked;
  }

  return config.IsNavigationAllowed(source, destination) ? Decision::kAllowed
                                                         : Decision::kBlocked;
}

}  // namespace origin_gating
