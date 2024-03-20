// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include <memory>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/string_utils.h"

namespace ash {
namespace {

#if !defined(NDEBUG)
constexpr base::TimeDelta kWatchdogTimeout = base::Seconds(5);
#else
constexpr base::TimeDelta kWatchdogTimeout = base::Seconds(10);
#endif

enum class EngineStatus {
  kStarting,
  kStarted,
  kFailed,
  kShuttingDown,
  kStopped
};

std::ostream& operator<<(std::ostream& out, EngineStatus status) {
  switch (status) {
#define PRINT(s)           \
  case EngineStatus::k##s: \
    return out << #s;
    PRINT(Starting)
    PRINT(Started)
    PRINT(Failed)
    PRINT(ShuttingDown)
    PRINT(Stopped)
#undef PRINT
  }
}

}  // namespace

struct AuthHubModeLifecycle::EngineState {
  std::unique_ptr<AuthFactorEngine> engine;
  EngineStatus status;
};

AuthHubModeLifecycle::AuthHubModeLifecycle(AuthHubModeLifecycle::Owner* owner)
    : owner_(owner) {}

AuthHubModeLifecycle::~AuthHubModeLifecycle() = default;

AuthHubModeLifecycle::Owner::~Owner() = default;

void AuthHubModeLifecycle::SwitchToMode(AuthHubMode target) {
  switch (stage_) {
    case Stage::kUninitialized:
      CHECK_EQ(mode_, AuthHubMode::kNone);
      if (target == AuthHubMode::kNone) {
        owner_->OnModeShutdown();
        return;
      }
      target_mode_ = target;
      initializing_for_mode_ = target_mode_;
      InitializeEnginesForMode();
      break;
    case Stage::kStarted:
      CHECK_NE(mode_, AuthHubMode::kNone);
      if (target != mode_) {
        target_mode_ = target;
        stage_ = Stage::kShuttingDownServices;
        ShutDownEngines();
      } else {
        LOG(WARNING) << "Multiple initialization to " << mode_;
      }
      return;
    case Stage::kStartingServices:
      // Set up new target mode, but do not set initializing_for_mode_,
      // it would trigger re-initialization.
      target_mode_ = target;
      break;
    case Stage::kShuttingDownServices:
      // Just update the target mode.
      target_mode_ = target;
      return;
  }
}

void AuthHubModeLifecycle::InitializeEnginesForMode() {
  CHECK(engines_.empty());
  CHECK_NE(target_mode_, AuthHubMode::kNone);

  for (const auto& factory : AuthParts::Get()->GetEngineFactories()) {
    auto engine = factory->CreateEngine(target_mode_);
    if (engine) {
      AshAuthFactor factor = factory->GetFactor();
      engines_[factor].engine = std::move(engine);
      engines_[factor].status = EngineStatus::kStarting;
    }
  }

  stage_ = Stage::kStartingServices;

  watchdog_.Stop();
  watchdog_.Start(
      FROM_HERE, kWatchdogTimeout,
      base::BindOnce(&AuthHubModeLifecycle::OnInitializationWatchdog,
                     weak_factory_.GetWeakPtr()));

  // TODO(b/277929602): metrics on initialization time.
  for (const auto& engine_state : engines_) {
    engine_state.second.engine->InitializeCommon(
        base::BindOnce(&AuthHubModeLifecycle::OnAuthEngineInitialized,
                       weak_factory_.GetWeakPtr()));
  }
}

void AuthHubModeLifecycle::OnAuthEngineInitialized(AshAuthFactor factor) {
  CHECK_EQ(stage_, Stage::kStartingServices);
  engines_[factor].status = EngineStatus::kStarted;
  CheckInitializationStatus();
}

void AuthHubModeLifecycle::OnInitializationWatchdog() {
  CHECK_EQ(stage_, Stage::kStartingServices);
  LOG(ERROR) << "Initialization watchdog triggered";
  // Invalidate all initialization callbacks:
  weak_factory_.InvalidateWeakPtrs();
  for (auto& engine_state : engines_) {
    if (engine_state.second.status == EngineStatus::kStarting) {
      engine_state.second.status = EngineStatus::kFailed;
      LOG(ERROR) << "Factor " << engine_state.first
                 << " did not initialize in time";
      engine_state.second.engine->InitializationTimedOut();
    }
  }
  CheckInitializationStatus();
}

void AuthHubModeLifecycle::CheckInitializationStatus() {
  bool all_initialized = true;
  for (const auto& engine_state : engines_) {
    switch (engine_state.second.status) {
      case EngineStatus::kStarting:
        all_initialized = false;
        break;
      case EngineStatus::kStarted:
      case EngineStatus::kFailed:
        break;
      case EngineStatus::kShuttingDown:
      case EngineStatus::kStopped:
        LOG(FATAL) << "Engine " << engine_state.first << " is in invalid state "
                   << engine_state.second.status;
    }
  }
  if (all_initialized) {
    watchdog_.Stop();

    if (target_mode_ != initializing_for_mode_) {
      // Trigger shutdown immediately; after shutdown is completed,
      // `CheckShutdownStatus` will trigger re-initialization to target_mode_.
      stage_ = Stage::kShuttingDownServices;
      ShutDownEngines();
      return;
    }

    initializing_for_mode_ = AuthHubMode::kNone;
    mode_ = target_mode_;
    stage_ = Stage::kStarted;

    owner_->OnReadyForMode(mode_, GetAvailableEngines());
  }
}

void AuthHubModeLifecycle::ShutDownEngines() {
  CHECK_EQ(stage_, Stage::kShuttingDownServices);
  for (auto& engine_state : engines_) {
    engine_state.second.status = EngineStatus::kShuttingDown;
  }

  watchdog_.Stop();
  watchdog_.Start(FROM_HERE, kWatchdogTimeout,
                  base::BindOnce(&AuthHubModeLifecycle::OnShutdownWatchdog,
                                 weak_factory_.GetWeakPtr()));

  // TODO(b/277929602): metrics on shutdown time.
  for (const auto& engine_state : engines_) {
    engine_state.second.engine->ShutdownCommon(
        base::BindOnce(&AuthHubModeLifecycle::OnAuthEngineShutdown,
                       weak_factory_.GetWeakPtr()));
  }
}

void AuthHubModeLifecycle::OnAuthEngineShutdown(AshAuthFactor factor) {
  CHECK_EQ(stage_, Stage::kShuttingDownServices);

  engines_[factor].status = EngineStatus::kStopped;

  CheckShutdownStatus();
}

void AuthHubModeLifecycle::OnShutdownWatchdog() {
  CHECK_EQ(stage_, Stage::kShuttingDownServices);
  LOG(ERROR) << "Shutdown watchdog triggered";
  // Invalidate all remaining shutdown callbacks:
  weak_factory_.InvalidateWeakPtrs();

  for (auto& engine_state : engines_) {
    if (engine_state.second.status == EngineStatus::kShuttingDown) {
      engine_state.second.status = EngineStatus::kFailed;
      LOG(ERROR) << "Factor " << engine_state.first
                 << " did not shut down in time";
      engine_state.second.engine->ShutdownTimedOut();
    }
  }
  CheckShutdownStatus();
}

void AuthHubModeLifecycle::CheckShutdownStatus() {
  CHECK_EQ(stage_, Stage::kShuttingDownServices);

  bool all_stopped = true;
  for (const auto& engine_state : engines_) {
    switch (engine_state.second.status) {
      case EngineStatus::kShuttingDown:
        all_stopped = false;
        break;
      case EngineStatus::kStopped:
      case EngineStatus::kFailed:
        break;
      case EngineStatus::kStarting:
      case EngineStatus::kStarted:
        LOG(FATAL) << "Engine " << engine_state.first << " is in invalid state "
                   << engine_state.second.status;
    }
  }

  if (all_stopped) {
    watchdog_.Stop();
    // Let owner release references to engines.
    if (mode_ != AuthHubMode::kNone) {
      owner_->OnExitedMode(mode_);
    }
    engines_.clear();
    mode_ = AuthHubMode::kNone;
    stage_ = Stage::kUninitialized;
    if (target_mode_ != AuthHubMode::kNone) {
      initializing_for_mode_ = target_mode_;
      InitializeEnginesForMode();
    } else {
      owner_->OnModeShutdown();
    }
  }
}

bool AuthHubModeLifecycle::IsReady() {
  return stage_ == Stage::kStarted;
}

AuthHubMode AuthHubModeLifecycle::GetCurrentMode() const {
  if (stage_ != Stage::kStarted) {
    return AuthHubMode::kNone;
  }
  return mode_;
}

AuthEnginesMap AuthHubModeLifecycle::GetAvailableEngines() {
  CHECK_EQ(stage_, Stage::kStarted);
  AuthEnginesMap result;
  for (const auto& engine_state : engines_) {
    if (engine_state.second.status == EngineStatus::kStarted) {
      result[engine_state.first] = engine_state.second.engine.get();
    }
  }
  return result;
}

}  // namespace ash
