// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_attempt_handler.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthHubAttemptHandler::AuthHubAttemptHandler(
    AuthHubAttemptHandler::Owner* owner,
    const AuthAttemptVector& attempt,
    const AuthEnginesMap& engines,
    AuthFactorsSet expected_factors)
    : owner_(owner),
      attempt_(attempt),
      engines_(engines),
      initial_factors_(expected_factors) {}

AuthHubAttemptHandler::~AuthHubAttemptHandler() = default;

AuthHubAttemptHandler::Owner::~Owner() = default;

AuthHubConnector* AuthHubAttemptHandler::GetConnector() {
  return this;
}

void AuthHubAttemptHandler::SetConsumer(
    raw_ptr<AuthFactorStatusConsumer> consumer) {
  status_consumer_ = std::move(consumer);
  status_consumer_->InitializeUi(initial_factors_, this);
}

bool AuthHubAttemptHandler::HasOngoingAttempt() const {
  // It is safe to proceed with shutdown if we have
  // successfully authenticated.
  if (authenticated_) {
    return false;
  }
  return ongoing_attempt_factor_.has_value();
}

void AuthHubAttemptHandler::PrepareForShutdown(base::OnceClosure callback) {
  CHECK(!callback.is_null());
  if (shutting_down_) {
    shutdown_callbacks_.AddUnsafe(std::move(callback));
    return;
  }
  if (!ongoing_attempt_factor_.has_value() || authenticated_) {
    shutting_down_ = true;
    UpdateAllFactorStates();
    std::move(callback).Run();
    return;
  }
  shutting_down_ = true;
  shutdown_callbacks_.AddUnsafe(std::move(callback));
}

void AuthHubAttemptHandler::OnFactorsChecked(AuthFactorsSet available_factors,
                                             AuthFactorsSet failed_factors) {
  DCHECK(Intersection(available_factors, failed_factors).empty());
  // TODO(b/286814076): Refine this strategy.
  // All factors can be split into 4 groups, according to initial_factors_ and
  // two parameters passed:
  //  * New factors, that are in `available_factors` but were not listed in
  //  `initial_factors_`;
  //  * Available factors, that present both  in `available_factors` and
  //  `initial_factors_`;
  //  * Removed factors, that were listed in `initial_factors_` but do not
  //  present neither in `available_factors` nor in `failed_factors`;
  //  * Failed factors;

  AuthFactorsSet potentially_present = Union(available_factors, failed_factors);
  AuthFactorsSet new_factors = Difference(available_factors, initial_factors_);
  AuthFactorsSet removed_factors =
      Difference(initial_factors_, potentially_present);

  // If some factor engines failed, but there are no new/removed factors,
  // report those factors in error state. Otherwise, consider them removed.

  AuthFactorsSet failed_initial =
      Intersection(initial_factors_, failed_factors);
  bool same_factor_list = new_factors.empty() && removed_factors.empty();

  if (same_factor_list) {
    for (AshAuthFactor f : failed_initial) {
      // Create entry, but mark as failed.
      factor_state_[f].engine_failed = true;
      CalculateFactorState(f, factor_state_[f]);
    }
  }

  // Retain only necessary engines, fill status for them:
  for (auto it = engines_.begin(); it != engines_.end();) {
    if (!available_factors.Has(it->first)) {
      it = engines_.erase(it);
      continue;
    }
    FillAllStatusValues(it->first, factor_state_[it->first]);
    it++;
  }

  FactorsStatusMap update;
  for (auto& state : factor_state_) {
    update[state.first] = state.second.internal_state;
    state.second.reported_state = state.second.internal_state;
  }

  if (same_factor_list) {
    status_consumer_->OnFactorStatusesChanged(update);
  } else {
    owner_->UpdateFactorUiCache(attempt_, available_factors);
    status_consumer_->OnFactorListChanged(update);
  }
  PropagateEnginesEnabledStatus();
}

void AuthHubAttemptHandler::PropagateStatusUpdates() {
  FactorsStatusMap update;
  for (auto& state : factor_state_) {
    if (state.second.internal_state != state.second.reported_state) {
      update[state.first] = state.second.internal_state;
      state.second.reported_state = state.second.internal_state;
    }
  }
  if (!update.empty()) {
    status_consumer_->OnFactorStatusesChanged(update);
  }
  PropagateEnginesEnabledStatus();
}

void AuthHubAttemptHandler::PropagateEnginesEnabledStatus() {
  for (auto& state : factor_state_) {
    if (state.second.intended_usage != state.second.engine_usage) {
      state.second.engine_usage = state.second.intended_usage;
      engines_[state.first]->SetUsageAllowed(state.second.engine_usage);
    }
  }
}

void AuthHubAttemptHandler::OnFactorPresenceChecked(AshAuthFactor factor,
                                                    bool factor_present) {
  // No-op, this method is implemented and handled by AuthHubVectorLifecycle.
  // Result would be provided to this class via `OnFactorsChecked` call.
}

void AuthHubAttemptHandler::OnFactorAttempt(AshAuthFactor factor) {
  ongoing_attempt_factor_ = factor;
  UpdateAllFactorStates();
}

void AuthHubAttemptHandler::UpdateAllFactorStates() {
  for (auto& state : factor_state_) {
    CalculateFactorState(state.first, state.second);
  }
  PropagateStatusUpdates();
}

void AuthHubAttemptHandler::OnFactorAttemptResult(AshAuthFactor factor,
                                                  bool success) {
  CHECK(ongoing_attempt_factor_.has_value());
  CHECK(factor == *ongoing_attempt_factor_);

  if (shutting_down_) {
    shutdown_callbacks_.Notify();
    return;
  }

  if (success) {
    status_consumer_->OnFactorAuthSuccess(factor);
    authenticated_ = true;
    // Keep an `ongoing_attempt_factor_` to prevent
    // factors from being re-enabled.
    status_consumer_->OnEndAuthentication();

    // Calling `OnEndAuthentication` signals the end of interaction with UI for
    // this particular attempt, which would eventually destroy UI, so we reset
    // the pointer here to avoid calling into a danling pointer.
    status_consumer_ = nullptr;

    // Signal the successful auth to every auth engine.
    for (const auto& [unused, engine] : engines_) {
      engine->OnSuccessfulAuthentiation();
    }

    owner_->OnAuthenticationSuccess(attempt_, factor);
    return;
  } else {
    status_consumer_->OnFactorAuthFailure(factor);
    owner_->OnFactorAttemptFailed(attempt_, factor);
    ongoing_attempt_factor_.reset();
  }
  UpdateAllFactorStates();
}

void AuthHubAttemptHandler::OnPolicyChanged(AshAuthFactor factor) {
  CHECK(factor_state_.contains(factor));
  auto& state = factor_state_[factor];
  if (state.engine_failed) {
    return;
  }
  CHECK(engines_.contains(factor));
  auto* engine = engines_[factor].get();
  state.disabled_by_policy = engine->IsDisabledByPolicy();
  CalculateFactorState(factor, state);
  PropagateStatusUpdates();
}

void AuthHubAttemptHandler::OnLockoutChanged(AshAuthFactor factor) {
  CHECK(factor_state_.contains(factor));
  auto& state = factor_state_[factor];
  if (state.engine_failed) {
    return;
  }
  CHECK(engines_.contains(factor));
  auto* engine = engines_[factor].get();
  state.locked_out = engine->IsLockedOut();
  CalculateFactorState(factor, state);
  PropagateStatusUpdates();
}

void AuthHubAttemptHandler::OnFactorSpecificRestrictionsChanged(
    AshAuthFactor factor) {
  CHECK(factor_state_.contains(factor));
  auto& state = factor_state_[factor];
  if (state.engine_failed) {
    return;
  }
  CHECK(engines_.contains(factor));
  auto* engine = engines_[factor].get();
  state.factor_specific_restricted = engine->IsFactorSpecificRestricted();
  CalculateFactorState(factor, state);
  PropagateStatusUpdates();
}

void AuthHubAttemptHandler::OnCriticalError(AshAuthFactor factor) {
  CHECK(factor_state_.contains(factor));
  factor_state_[factor].engine_failed = true;
  CalculateFactorState(factor, factor_state_[factor]);
  PropagateStatusUpdates();
}

void AuthHubAttemptHandler::OnFactorCustomSignal(AshAuthFactor factor) {
  CHECK(engines_.contains(factor));
  status_consumer_->OnFactorCustomSignal(factor);
}

void AuthHubAttemptHandler::FillAllStatusValues(AshAuthFactor factor,
                                                FactorAttemptState& state) {
  CHECK(engines_.contains(factor));
  auto* engine = engines_[factor].get();
  state.disabled_by_policy = engine->IsDisabledByPolicy();
  state.locked_out = engine->IsLockedOut();
  state.factor_specific_restricted = engine->IsFactorSpecificRestricted();
  CalculateFactorState(factor, state);
}

void AuthHubAttemptHandler::CalculateFactorState(AshAuthFactor factor,
                                                 FactorAttemptState& state) {
  state.internal_state = AuthFactorState::kFactorReady;
  if (state.engine_failed) {
    state.internal_state = AuthFactorState::kEngineError;
    // Factor is marked as Failed if it's engine did not start.
    // We do not modify `intended_usage` here as we can not propagate
    // it to the engine.
    return;
  }
  if (shutting_down_) {
    // We need to set some disabled state here to prevent factors from
    // being used, does not matter which one.
    state.internal_state = AuthFactorState::kDisabledParallelAttempt;
    // As code is in the shutdown sequence, engine will not be re-enabled
    // again, so use kDisabled instead of kDisabledParallelAttempt here,
    // to prevent engine from queueing any attempts.
    state.intended_usage = AuthFactorEngine::UsageAllowed::kDisabled;
    return;
  }
  if (state.disabled_by_policy) {
    state.internal_state = AuthFactorState::kDisabledByPolicy;
    state.intended_usage = AuthFactorEngine::UsageAllowed::kDisabled;
    return;
  }
  if (state.factor_specific_restricted) {
    state.internal_state = AuthFactorState::kDisabledFactorSpecific;
    state.intended_usage = AuthFactorEngine::UsageAllowed::kDisabled;
    return;
  }
  if (state.locked_out) {
    state.internal_state = AuthFactorState::kLockedOutIndefinite;
    state.intended_usage = AuthFactorEngine::UsageAllowed::kDisabled;
    return;
  }
  if (ongoing_attempt_factor_.has_value()) {
    if (*ongoing_attempt_factor_ == factor) {
      state.internal_state = AuthFactorState::kOngoingAttempt;
    } else {
      state.internal_state = AuthFactorState::kDisabledParallelAttempt;
    }
    // While there is an ongoing attempt, keep all factors disabled to
    // prevent double authentication.
    state.intended_usage =
        AuthFactorEngine::UsageAllowed::kDisabledParallelAttempt;
    return;
  }
  state.internal_state = AuthFactorState::kFactorReady;
  state.intended_usage = AuthFactorEngine::UsageAllowed::kEnabled;
}

AuthFactorEngine* AuthHubAttemptHandler::GetEngine(AshAuthFactor factor) {
  CHECK(engines_.contains(factor));
  return engines_[factor];
}

}  // namespace ash
