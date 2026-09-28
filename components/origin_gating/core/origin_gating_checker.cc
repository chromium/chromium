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
#include "base/types/expected_macros.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/task_policy_config_slot.h"
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

Decision EvaluateForbidNonLocalhostIpAddress(const GURL& destination) {
  return destination.HostIsIPAddress() && !net::IsLocalhost(destination)
             ? Decision::kBlocked
             : Decision::kNoDecision;
}

Decision EvaluateRequireHttpsOrLocalhost(const GURL& destination) {
  return destination.SchemeIs(url::kHttpsScheme) ||
                 (net::IsLocalhost(destination) &&
                  destination.SchemeIs(url::kHttpScheme))
             ? Decision::kNoDecision
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

DecisionAttribution MakeAttribution(DecisionSource source) {
  return DecisionAttribution(source);
}

DecisionAttribution MakeAttribution(const CustomPredicate& predicate) {
  return DecisionAttribution(predicate.attribution());
}

Decision EvaluateActorContainerConfig(
    const TaskPolicyConfigSlot& config_slot,
    const GateableEvent& event,
    const std::optional<url::Origin>& source_origin,
    const url::Origin& destination_origin) {
  if (!config_slot.has_value()) {
    return Decision::kNoDecision;
  }
  const TaskPolicyConfig& config = config_slot.value();
  if (event.GetIfPageAction()) {
    return config.IsActuationAllowed(destination_origin) ? Decision::kAllowed
                                                         : Decision::kBlocked;
  }

  CHECK(source_origin.has_value());
  return config.IsNavigationAllowed(*source_origin, destination_origin)
             ? Decision::kAllowed
             : Decision::kBlocked;
}

}  // namespace

OriginGatingChecker::OriginGatingChecker(base::WeakPtr<Delegate> delegate,
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
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<url::Origin> source_origin =
      event.source() ? std::make_optional(url::Origin::Create(*event.source()))
                     : std::nullopt;
  url::Origin destination_origin = url::Origin::Create(event.destination());
  EvaluatePredicates(std::move(context), config_.predicates(),
                     DelegateInputs{
                         .event = std::move(event),
                         .source_origin = std::move(source_origin),
                         .destination_origin = std::move(destination_origin),
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

  if (!delegate_) {
    return;
  }

  for (size_t i = 0; i < pending_predicates.size(); ++i) {
    const PredicateConfiguration& predicate_config = pending_predicates[i];
    if (!predicate_config.AppliesTo(input.event.type())) {
      continue;
    }

    ASSIGN_OR_RETURN(
        Decision decision,
        std::visit(
            [&](auto&& predicate) VALID_CONTEXT_REQUIRED(sequence_checker_) {
              return EvaluateSinglePredicate(context,
                                             pending_predicates.subspan(i),
                                             predicate, input, callback);
            },
            predicate_config.predicate()),
        [] {});

    DecisionAttribution attribution =
        std::visit([](auto&& predicate) { return MakeAttribution(predicate); },
                   predicate_config.predicate());
    if (ProcessDecision(context, attribution, decision, callback)) {
      return;
    }
  }

  RunActionOrGetUserConfirmationInfo(
      context, /*pending_predicates=*/{}, input, callback,
      [&]() VALID_CONTEXT_REQUIRED(sequence_checker_) {
        if (!delegate_) {
          return;
        }
        GatingDecisionContext* raw_context = context.get();
        GateableEvent event = input.event;
        bool requires_user_confirmation =
            input.requires_user_confirmation.value();
        delegate_->OnNoVerdict(
            raw_context, event, requires_user_confirmation,
            base::BindOnce(&OriginGatingChecker::OnNoVerdictAnswer,
                           weak_ptr_factory_.GetWeakPtr(), std::move(context),
                           std::move(input), std::move(callback)));
      });
}

std::optional<Decision> OriginGatingChecker::EvaluateSinglePredicate(
    std::unique_ptr<GatingDecisionContext>& context,
    base::span<const PredicateConfiguration> current_and_rest,
    DecisionSource decision_source,
    DelegateInputs& input,
    GatingDecisionCallback& callback) {
  switch (decision_source) {
    case DecisionSource::kAllowSameOrigin:
      CHECK(input.source_origin.has_value());
      return EvaluateAllowSameOrigin(*input.source_origin,
                                     input.destination_origin);
    case DecisionSource::kAllowHttpLocalhost:
      return EvaluateAllowHttpLocalhost(input.event.destination());
    case DecisionSource::kAllowAboutBlank:
      return EvaluateAllowAboutBlank(input.event.destination());
    case DecisionSource::kCacheWithUserConfirmation:
      return IsCachedWithUserConfirmation(input.destination_origin);
    case DecisionSource::kCacheWithoutUserConfirmation: {
      std::optional<Decision> decision;
      RunActionOrGetUserConfirmationInfo(
          context, current_and_rest, input, callback,
          [&]() VALID_CONTEXT_REQUIRED(sequence_checker_) {
            decision = !input.requires_user_confirmation.value() &&
                               cache_.IsNavigationAllowed(
                                   input.source_origin.value_or(url::Origin()),
                                   input.destination_origin)
                           ? Decision::kAllowed
                           : Decision::kNoDecision;
          });
      return decision;
    }
    case DecisionSource::kEnterprisePolicy: {
      GURL destination = input.event.destination();
      delegate_->EvaluateEnterprisePolicy(
          destination,
          base::BindOnce(&OriginGatingChecker::OnEnterprisePolicyVerdict,
                         weak_ptr_factory_.GetWeakPtr(), std::move(context),
                         current_and_rest.subspan(1U),
                         DecisionAttribution(decision_source), std::move(input),
                         std::move(callback)));
      return std::nullopt;
    }
    case DecisionSource::kForbidNonLocalhostIpAddress:
      return EvaluateForbidNonLocalhostIpAddress(input.event.destination());
    case DecisionSource::kRequireHttpsOrLocalhost:
      return EvaluateRequireHttpsOrLocalhost(input.event.destination());
    case DecisionSource::kRequireHttpsOrHttp:
      return EvaluateRequireHttpsOrHttp(input.event.destination());
    case DecisionSource::kBlockByTaskPolicyConfig:
      return EvaluateTaskPolicyConfigWithCache(input) == Decision::kBlocked
                 ? Decision::kBlocked
                 : Decision::kNoDecision;
    case DecisionSource::kAllowByTaskPolicyConfig:
      return EvaluateTaskPolicyConfigWithCache(input) == Decision::kAllowed
                 ? Decision::kAllowed
                 : Decision::kNoDecision;
    case DecisionSource::kNoVerdict:
      // This is an internal/fallback decision source and is not an
      // executable predicate. OriginGatingConfiguration's
      // constructor guarantees that this is never present in the
      // predicates list, making this block unreachable.
      NOTREACHED();
  }
  NOTREACHED();
}

std::optional<Decision> OriginGatingChecker::EvaluateSinglePredicate(
    std::unique_ptr<GatingDecisionContext>& context,
    base::span<const PredicateConfiguration> current_and_rest,
    const CustomPredicate& custom_predicate,
    DelegateInputs& input,
    GatingDecisionCallback& callback) {
  GatingDecisionContext* raw_context = context.get();
  return std::visit(
      absl::Overload{
          [&](const CustomPredicate::AsyncPredicate& predicate)
              VALID_CONTEXT_REQUIRED(
                  sequence_checker_) -> std::optional<Decision> {
                GateableEvent event = input.event;
                predicate.Run(
                    raw_context, event,
                    base::BindOnce(
                        &OriginGatingChecker::OnEvaluatedAsyncPredicate,
                        weak_ptr_factory_.GetWeakPtr(), std::move(context),
                        current_and_rest.subspan(1U),
                        MakeAttribution(custom_predicate), std::move(input),
                        std::move(callback)));
                return std::nullopt;
              },
          [&](const CustomPredicate::SyncPredicate& predicate)
              -> std::optional<Decision> {
            return predicate.Run(raw_context, input.event);
          },
      },
      custom_predicate.predicate());
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

void OriginGatingChecker::RunActionOrGetUserConfirmationInfo(
    std::unique_ptr<GatingDecisionContext>& context,
    base::span<const PredicateConfiguration> pending_predicates,
    DelegateInputs& input,
    GatingDecisionCallback& callback,
    base::FunctionRef<void()> action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (input.requires_user_confirmation.has_value()) {
    return action();
  }
  if (!delegate_) {
    return;
  }
  GatingDecisionContext* raw_context = context.get();
  GateableEvent event = input.event;
  delegate_->DoesOriginRequireUserConfirmation(
      raw_context, event,
      base::BindOnce(&OriginGatingChecker::OnUserConfirmationRequiredAnswer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     pending_predicates, std::move(input),
                     std::move(callback)));
}

Decision OriginGatingChecker::IsCachedWithUserConfirmation(
    const url::Origin& origin) const {
  return cache_.IsNavigationConfirmedByUser(origin) ? Decision::kAllowed
                                                    : Decision::kNoDecision;
}

Decision OriginGatingChecker::EvaluateTaskPolicyConfigWithCache(
    DelegateInputs& input) const {
  if (!input.actor_container_decision.has_value()) {
    input.actor_container_decision = EvaluateActorContainerConfig(
        task_policy_config_slot_, input.event, input.source_origin,
        input.destination_origin);
  }
  return input.actor_container_decision.value();
}

}  // namespace origin_gating
