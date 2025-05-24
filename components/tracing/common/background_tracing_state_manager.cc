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
}

void BackgroundTracingStateManager::SaveState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);

  base::Value::Dict dict;

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

void BackgroundTracingStateManager::UpdateEnabledScenarios(
    std::vector<std::string> enabled_scenarios) {
  enabled_scenarios_ = std::move(enabled_scenarios);
  SaveState();
}

void BackgroundTracingStateManager::UpdatePrivacyFilter(bool enabled) {
  privacy_filter_enabled_ = enabled;
  SaveState();
}

void BackgroundTracingStateManager::ResetForTesting() {
  enabled_scenarios_ = {};
  privacy_filter_enabled_ = false;
  Initialize();
}

}  // namespace tracing
