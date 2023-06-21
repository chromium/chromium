// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_vector_lifecycle.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/string_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// TODO (b/271248265): Review timeout values.
#if !defined(NDEBUG)
constexpr base::TimeDelta kWatchdogTimeout = base::Seconds(5);
#else
constexpr base::TimeDelta kWatchdogTimeout = base::Seconds(10);
#endif

enum class EngineAttemptStatus {
  kIdle,
  kStarting,
  kNoFactor,
  kStarted,
  kFailed,
  kCriticalError,
  kFinishing,
};

}  // namespace

struct AuthHubVectorLifecycle::FactorAttemptState {
  base::raw_ptr<AuthFactorEngine, DanglingUntriaged> engine;
  EngineAttemptStatus status;
};

AuthHubVectorLifecycle::AuthHubVectorLifecycle(Owner* owner,
                                               AuthHubMode mode,
                                               const AuthEnginesMap& engines)
    : available_engines_(engines), owner_(owner) {
  CHECK(!engines.empty());
}

AuthHubVectorLifecycle::~AuthHubVectorLifecycle() = default;

AuthHubVectorLifecycle::Owner::~Owner() = default;

void AuthHubVectorLifecycle::StartAttempt(const AuthAttemptVector& attempt) {
  target_attempt_ = attempt;
  switch (stage_) {
    case Stage::kIdle:
      StartForTargetAttempt();
      break;
    case Stage::kStarted:
      FinishAttempt();
      break;
    case Stage::kStartingAttempt:
      // Set up new target mode, but do not modify initializing_for_,
      // `ProceedIfAllFactorsStarted` would trigger re-initialization.
      break;
    case Stage::kFinishingAttempt:
      // Just update the target mode, `ProceedIfAllFactorsFinished` would
      // trigger new attempt.
      break;
  }
}

void AuthHubVectorLifecycle::CancelAttempt() {
  switch (stage_) {
    case Stage::kIdle:
      LOG(WARNING) << "Request to cancel attempt without actual attempt";
      break;
    case Stage::kStarted:
      target_attempt_ = absl::nullopt;
      FinishAttempt();
      break;
    case Stage::kStartingAttempt:
    case Stage::kFinishingAttempt:
      target_attempt_ = absl::nullopt;
      break;
  }
}

bool AuthHubVectorLifecycle::IsIdle() const {
  return stage_ == Stage::kIdle;
}

void AuthHubVectorLifecycle::StartForTargetAttempt() {
  CHECK_EQ(stage_, Stage::kIdle);
  CHECK(target_attempt_.has_value());

  stage_ = Stage::kStartingAttempt;
  initializing_for_ = target_attempt_;

  CHECK(engines_.empty());
  for (const auto& engine : available_engines_) {
    engines_[engine.first] =
        FactorAttemptState{engine.second, EngineAttemptStatus::kStarting};
  }

  watchdog_.Stop();
  watchdog_.Start(
      FROM_HERE, kWatchdogTimeout,
      base::BindOnce(&AuthHubVectorLifecycle::OnAttemptStartWatchdog,
                     weak_factory_.GetWeakPtr()));
  for (auto& state : engines_) {
    state.second.engine->StartAuthFlow(target_attempt_->account,
                                       target_attempt_->purpose, this);
  }
}

void AuthHubVectorLifecycle::OnAttemptStartWatchdog() {
  LOG(ERROR) << "Attempt start watchdog triggered";
  CHECK_EQ(stage_, Stage::kStartingAttempt);

  for (auto& state : engines_) {
    if (state.second.status == EngineAttemptStatus::kStarting) {
      state.second.status = EngineAttemptStatus::kFailed;
      LOG(ERROR) << "Factor " << state.first << " did not start in time";
      state.second.engine->StartFlowTimedOut();
    }
  }
  ProceedIfAllFactorsStarted();
}

void AuthHubVectorLifecycle::ProceedIfAllFactorsStarted() {
  CHECK_EQ(stage_, Stage::kStartingAttempt);
  for (const auto& state : engines_) {
    if (state.second.status == EngineAttemptStatus::kStarting) {
      return;
    }
  }
  watchdog_.Stop();
  stage_ = Stage::kStarted;
  if (initializing_for_ != target_attempt_) {
    // Not notifying owner, just restart for new target.
    initializing_for_ = absl::nullopt;
    FinishAttempt();
    return;
  }
  CHECK(target_attempt_.has_value());

  current_attempt_ = target_attempt_;

  AuthFactorsSet present;
  AuthFactorsSet failed;

  for (const auto& state : engines_) {
    switch (state.second.status) {
      case EngineAttemptStatus::kStarted:
        present.Put(state.first);
        state.second.engine->UpdateObserver(owner_->AsEngineObserver());
        break;
      case EngineAttemptStatus::kNoFactor:
        // Just ignore them
        break;
      case EngineAttemptStatus::kCriticalError:
      case EngineAttemptStatus::kFailed:
        failed.Put(state.first);
        break;
      case EngineAttemptStatus::kStarting:
      case EngineAttemptStatus::kIdle:
      case EngineAttemptStatus::kFinishing:
        NOTREACHED_NORETURN();
    }
  }
  owner_->OnAttemptStarted(*current_attempt_, present, failed);
}

void AuthHubVectorLifecycle::FinishAttempt() {
  CHECK_EQ(stage_, Stage::kStarted);
  stage_ = Stage::kFinishingAttempt;
  for (auto& state : engines_) {
    state.second.status = EngineAttemptStatus::kFinishing;
  }
  watchdog_.Stop();
  watchdog_.Start(
      FROM_HERE, kWatchdogTimeout,
      base::BindOnce(&AuthHubVectorLifecycle::OnAttemptFinishWatchdog,
                     weak_factory_.GetWeakPtr()));

  // TODO(b/277929602): metrics on initialization time.
  for (auto& state : engines_) {
    state.second.engine->UpdateObserver(this);
    state.second.engine->StopAuthFlow(base::BindOnce(
        &AuthHubVectorLifecycle::OnFactorFinished, weak_factory_.GetWeakPtr()));
  }
}

void AuthHubVectorLifecycle::OnFactorFinished(AshAuthFactor factor) {
  CHECK_EQ(stage_, Stage::kFinishingAttempt);
  CHECK(engines_.contains(factor));
  engines_[factor].status = EngineAttemptStatus::kIdle;
  engines_[factor].engine->UpdateObserver(nullptr);
  ProceedIfAllFactorsFinished();
}

void AuthHubVectorLifecycle::OnAttemptFinishWatchdog() {
  LOG(ERROR) << "Attempt finish watchdog triggered";
  CHECK_EQ(stage_, Stage::kFinishingAttempt);

  for (auto& state : engines_) {
    if (state.second.status == EngineAttemptStatus::kFinishing) {
      state.second.status = EngineAttemptStatus::kFailed;
      LOG(ERROR) << "Factor " << state.first << " did not finish in time";
      state.second.engine->StopFlowTimedOut();
      state.second.engine->UpdateObserver(nullptr);
    }
  }
  ProceedIfAllFactorsFinished();
}

void AuthHubVectorLifecycle::ProceedIfAllFactorsFinished() {
  CHECK_EQ(stage_, Stage::kFinishingAttempt);
  for (const auto& state : engines_) {
    if (state.second.status == EngineAttemptStatus::kFinishing) {
      return;
    }
  }
  engines_.clear();

  watchdog_.Stop();
  stage_ = Stage::kIdle;

  if (current_attempt_.has_value()) {
    // We have notified owner about attempt start, so
    // we need to notify about finish.
    owner_->OnAttemptFinished(*current_attempt_);
  }
  current_attempt_ = absl::nullopt;

  if (target_attempt_.has_value()) {
    StartForTargetAttempt();
  } else {
    owner_->OnIdle();
  }
}

// AuthFactorEngine::FactorEngineObserver:
void AuthHubVectorLifecycle::OnFactorPresenceChecked(AshAuthFactor factor,
                                                     bool factor_present) {
  CHECK_EQ(stage_, Stage::kStartingAttempt);
  CHECK(engines_.contains(factor));

  engines_[factor].status = factor_present ? EngineAttemptStatus::kStarted
                                           : EngineAttemptStatus::kNoFactor;
  ProceedIfAllFactorsStarted();
}

void AuthHubVectorLifecycle::OnFactorAttempt(AshAuthFactor factor) {
  // Ignored
  // Should not happen, as factors start in disabled state.
  base::debug::DumpWithoutCrashing();
}
void AuthHubVectorLifecycle::OnFactorAttemptResult(AshAuthFactor factor,
                                                   bool success) {
  // Ignored
  // Should not happen, as factors start in disabled state.
  base::debug::DumpWithoutCrashing();
}

void AuthHubVectorLifecycle::OnPolicyChanged(AshAuthFactor factor) {
  // Ignored
}
void AuthHubVectorLifecycle::OnLockoutChanged(AshAuthFactor factor) {
  // Ignored
}
void AuthHubVectorLifecycle::OnFactorSpecificRestrictionsChanged(
    AshAuthFactor factor) {
  // Ignored
}
void AuthHubVectorLifecycle::OnFactorCustomSignal(AshAuthFactor factor) {
  // Ignored
}

void AuthHubVectorLifecycle::OnCriticalError(AshAuthFactor factor) {
  CHECK(stage_ == Stage::kStartingAttempt ||
        stage_ == Stage::kFinishingAttempt);

  CHECK(engines_.contains(factor));
  engines_[factor].status = EngineAttemptStatus::kCriticalError;

  if (stage_ == Stage::kStartingAttempt) {
    ProceedIfAllFactorsStarted();
  } else {
    ProceedIfAllFactorsFinished();
  }
}

}  // namespace ash
