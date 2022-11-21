// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_state_manager.h"

#include "base/json/values_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/tracing/common/pref_names.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/browser_thread.h"

namespace {

constexpr char kTracingStateKey[] = "state";
constexpr char kUploadTimesKey[] = "upload_times";
constexpr char kScenarioKey[] = "scenario";
constexpr char kUploadTimestampKey[] = "time";

const int kMinDaysUntilNextUpload = 7;

// Removes any version numbers from the scenario name.
std::string StripScenarioName(const std::string& scenario_name) {
  std::string stripped_scenario_name;
  base::RemoveChars(scenario_name, "1234567890", &stripped_scenario_name);
  return stripped_scenario_name;
}

}  // namespace

namespace tracing {

BackgroundTracingStateManager::BackgroundTracingStateManager() = default;
BackgroundTracingStateManager::~BackgroundTracingStateManager() = default;

BackgroundTracingStateManager& BackgroundTracingStateManager::GetInstance() {
  static base::NoDestructor<BackgroundTracingStateManager> instance;
  return *instance;
}

void BackgroundTracingStateManager::SetPrefServiceForTesting(
    PrefService* local_state) {
  if (!local_state_ && local_state)
    local_state_ = local_state;
}

void BackgroundTracingStateManager::Initialize(PrefService* local_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (initialized_)
    return;

  initialized_ = true;

  if (!local_state_ && local_state)
    local_state_ = local_state;

  DCHECK(local_state_);
  const base::Value::Dict& dict =
      local_state_->GetDict(kBackgroundTracingSessionState);

  absl::optional<int> state = dict.FindInt(kTracingStateKey);

  if (state) {
    if (*state >= 0 &&
        *state <= static_cast<int>(BackgroundTracingState::LAST)) {
      last_session_end_state_ = static_cast<BackgroundTracingState>(*state);
    } else {
      last_session_end_state_ = BackgroundTracingState::NOT_ACTIVATED;
    }
  }

  const base::Value::List* upload_times = dict.FindList(kUploadTimesKey);
  if (upload_times) {
    for (const auto& scenario_dict : *upload_times) {
      DCHECK(scenario_dict.is_dict());
      const std::string* scenario = scenario_dict.FindStringKey(kScenarioKey);
      const base::Value* timestamp_val =
          scenario_dict.FindKey(kUploadTimestampKey);
      if (!scenario || !timestamp_val) {
        continue;
      }

      absl::optional<base::Time> upload_time = base::ValueToTime(timestamp_val);
      if (!upload_time) {
        continue;
      }

      if ((base::Time::Now() - *upload_time) >
          base::Days(kMinDaysUntilNextUpload)) {
        continue;
      }
      scenario_last_upload_timestamp_[*scenario] = *upload_time;
    }
  }

  // Save state to update the current session state, replacing the previous
  // session state.
  SaveState();
}

void BackgroundTracingStateManager::SaveState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
  SaveState(scenario_last_upload_timestamp_, state_);
}

void BackgroundTracingStateManager::SaveState(
    const ScenarioUploadTimestampMap& scenario_upload_times,
    BackgroundTracingState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey(kTracingStateKey, static_cast<int>(state));
  base::Value upload_times(base::Value::Type::LIST);
  for (const auto& it : scenario_upload_times) {
    base::Value scenario(base::Value::Type::DICTIONARY);
    scenario.SetStringKey(kScenarioKey, StripScenarioName(it.first));
    scenario.SetKey(kUploadTimestampKey, base::TimeToValue(it.second));
    upload_times.Append(std::move(scenario));
  }

  dict.SetKey(kUploadTimesKey, std::move(upload_times));

  local_state_->Set(kBackgroundTracingSessionState, std::move(dict));
  local_state_->CommitPendingWrite();
}

bool BackgroundTracingStateManager::DidLastSessionEndUnexpectedly() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
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

bool BackgroundTracingStateManager::DidRecentlyUploadForScenario(
    const content::BackgroundTracingConfig& config) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
  std::string stripped_scenario_name =
      StripScenarioName(config.scenario_name());
  auto it = scenario_last_upload_timestamp_.find(stripped_scenario_name);
  if (it != scenario_last_upload_timestamp_.end()) {
    return (base::Time::Now() - it->second) <=
           base::Days(kMinDaysUntilNextUpload);
  }
  return false;
}

void BackgroundTracingStateManager::NotifyTracingStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetState(BackgroundTracingState::STARTED);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce([]() {
        BackgroundTracingStateManager::GetInstance().SetState(
            BackgroundTracingState::RAN_30_SECONDS);
      }),
      base::Seconds(30));
}

void BackgroundTracingStateManager::NotifyFinalizationStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetState(BackgroundTracingState::FINALIZATION_STARTED);
}

void BackgroundTracingStateManager::SetState(BackgroundTracingState new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
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

void BackgroundTracingStateManager::OnScenarioUploaded(
    const std::string& scenario_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);

  scenario_last_upload_timestamp_[StripScenarioName(scenario_name)] =
      base::Time::Now();
  SaveState();
}

void BackgroundTracingStateManager::Reset() {
  initialized_ = false;
  local_state_ = nullptr;
  state_ = BackgroundTracingState::NOT_ACTIVATED;
  last_session_end_state_ = BackgroundTracingState::NOT_ACTIVATED;
  scenario_last_upload_timestamp_.clear();
}

}  // namespace tracing
