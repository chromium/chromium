// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_state_manager.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/tracing/common/pref_names.h"

namespace tracing {
namespace {

constexpr char kTracingStateKey[] = "state";
constexpr char kTracingEnabledScenariosKey[] = "enabled_scenarios";
constexpr char kTracingPrivacyFilterKey[] = "privacy_filter";

BackgroundTracingStateManager* g_background_tracing_state_manager = nullptr;

}  // namespace

BackgroundTracingStateManager::BackgroundTracingStateManager(
    PrefService* local_state)
    : local_state_(local_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(g_background_tracing_state_manager, nullptr);
  g_background_tracing_state_manager = this;

  Initialize();
}

BackgroundTracingStateManager::~BackgroundTracingStateManager() {
  DCHECK_EQ(g_background_tracing_state_manager, this);
  g_background_tracing_state_manager = nullptr;
}

std::unique_ptr<BackgroundTracingStateManager>
BackgroundTracingStateManager::CreateInstance(PrefService* local_state) {
  if (local_state == nullptr) {
    return nullptr;
  }
  return base::WrapUnique(new BackgroundTracingStateManager(local_state));
}

BackgroundTracingStateManager& BackgroundTracingStateManager::GetInstance() {
  CHECK_NE(nullptr, g_background_tracing_state_manager);
  return *g_background_tracing_state_manager;
}

void BackgroundTracingStateManager::Initialize() {
  DCHECK(local_state_);

  const base::Value::Dict& dict =
      local_state_->GetDict(kBackgroundTracingSessionState);

  std::optional<int> state = dict.FindInt(kTracingStateKey);

  if (state) {
    if (*state >= 0 &&
        *state <= static_cast<int>(BackgroundTracingState::LAST)) {
      last_session_end_state_ = static_cast<BackgroundTracingState>(*state);
    } else {
      last_session_end_state_ = BackgroundTracingState::NOT_ACTIVATED;
    }
  }

  auto* scenarios = dict.FindList(kTracingEnabledScenariosKey);
  if (scenarios) {
    for (const auto& item : *scenarios) {
      auto* scenario_hash = item.GetIfString();
      if (scenario_hash) {
        enabled_scenarios_.push_back(*scenario_hash);
      }
    }
  }

  std::optional<bool> privacy_filter_enabled =
      dict.FindBool(kTracingPrivacyFilterKey);
  if (privacy_filter_enabled) {
    privacy_filter_enabled_ = *privacy_filter_enabled;
  }

  // Save state to update the current session state, replacing the previous
  // session state.
  SaveState();
}

void BackgroundTracingStateManager::SaveState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);

  base::Value::Dict dict;
  dict.Set(kTracingStateKey, static_cast<int>(state_));

  if (!enabled_scenarios_.empty()) {
    base::Value::List scenarios;
    for (const auto& scenario_name : enabled_scenarios_) {
      scenarios.Append(scenario_name);
    }
    dict.Set(kTracingEnabledScenariosKey, std::move(scenarios));
  }
  dict.Set(kTracingPrivacyFilterKey, privacy_filter_enabled_);

  local_state_->SetDict(kBackgroundTracingSessionState, std::move(dict));
  local_state_->CommitPendingWrite();
}

bool BackgroundTracingStateManager::DidLastSessionEndUnexpectedly() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (last_session_end_state_) {
    case BackgroundTracingState::NOT_ACTIVATED:
    case BackgroundTracingState::RAN_30_SECONDS:
    case BackgroundTracingState::FINALIZATION_STARTED:
      return false;
    case BackgroundTracingState::STARTED:
      // If the browser did not run for 30 seconds after tracing started in
      // previous session then do not start tracing in current session as a
      // safeguard. This would be impacted by short sessions (eg: on Android),
      // but worth the tradeoff of crashing loop on startup. Checking for
      // previous session crash status is platform dependent and the crash
      // status is initialized at later point than when tracing begins. So, this
      // check is safer than waiting for crash metrics to be available. Note
      // that this setting only checks for last session and not sessions before
      // that. So, the next session might still crash due to tracing if the user
      // has another tracing experiment. But, meanwhile we would be able to turn
      // off tracing experiments based on uploaded crash metrics.
      return true;
  }
}

void BackgroundTracingStateManager::OnTracingStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(BackgroundTracingState::STARTED);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce([]() {
        BackgroundTracingStateManager::GetInstance().SetState(
            BackgroundTracingState::RAN_30_SECONDS);
      }),
      base::Seconds(30));
}

void BackgroundTracingStateManager::OnTracingStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(BackgroundTracingState::FINALIZATION_STARTED);
}

void BackgroundTracingStateManager::UpdateEnabledScenarios(
    std::vector<std::string> enabled_scenarios) {
  enabled_scenarios_ = std::move(enabled_scenarios);
  SaveState();
}

void BackgroundTracingStateManager::UpdatePrivacyFilter(bool enabled) {
  privacy_filter_enabled_ = enabled;
  SaveState();
}

void BackgroundTracingStateManager::SetState(BackgroundTracingState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == new_state) {
    return;
  }
  // If finalization started before 30 seconds, skip recording the new state.
  if (new_state == BackgroundTracingState::RAN_30_SECONDS &&
      state_ == BackgroundTracingState::FINALIZATION_STARTED) {
    return;
  }
  state_ = new_state;
  SaveState();
}

void BackgroundTracingStateManager::ResetForTesting() {
  state_ = BackgroundTracingState::NOT_ACTIVATED;
  last_session_end_state_ = BackgroundTracingState::NOT_ACTIVATED;
  enabled_scenarios_ = {};
  privacy_filter_enabled_ = true;
  Initialize();
}

}  // namespace tracing
