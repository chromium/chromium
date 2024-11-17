// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cloned_install_detector.h"

#include <stdint.h>

#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/metrics/machine_id_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace {

uint32_t HashRawId(const std::string& value) {
  uint64_t hash = base::HashMetricName(value);

  // Only use 24 bits from the 64-bit hash.
  return hash & ((1 << 24) - 1);
}

// State of the generated machine id in relation to the previously stored value.
// Note: UMA histogram enum - don't re-order or remove entries
enum MachineIdState {
  ID_GENERATION_FAILED,
  ID_NO_STORED_VALUE,
  ID_CHANGED,
  ID_UNCHANGED,
  ID_ENUM_SIZE
};

// Logs the state of generating a machine id and comparing it to a stored value.
void LogMachineIdState(MachineIdState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.MachineIdState", state, ID_ENUM_SIZE);
}

}  // namespace

ClonedInstallDetector::ClonedInstallDetector() = default;

ClonedInstallDetector::~ClonedInstallDetector() {
}

void ClonedInstallDetector::CheckForClonedInstall(PrefService* local_state) {
  if (!MachineIdProvider::HasId())
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&MachineIdProvider::GetMachineId),
      base::BindOnce(&ClonedInstallDetector::SaveMachineId,
                     weak_ptr_factory_.GetWeakPtr(), local_state));
}

void ClonedInstallDetector::SaveMachineId(PrefService* local_state,
                                          const std::string& raw_id) {
  if (raw_id.empty()) {
    LogMachineIdState(ID_GENERATION_FAILED);
    local_state->ClearPref(prefs::kMetricsMachineId);
    return;
  }

  int hashed_id = HashRawId(raw_id);

  MachineIdState id_state = ID_NO_STORED_VALUE;
  if (local_state->HasPrefPath(prefs::kMetricsMachineId)) {
    if (local_state->GetInteger(prefs::kMetricsMachineId) != hashed_id) {
      DCHECK(!detected_this_session_);
      id_state = ID_CHANGED;
      detected_this_session_ = true;
      local_state->SetBoolean(prefs::kMetricsResetIds, true);
      callback_list_.Notify();
    } else {
      id_state = ID_UNCHANGED;
    }
  }

  LogMachineIdState(id_state);

  local_state->SetInteger(prefs::kMetricsMachineId, hashed_id);
}

bool ClonedInstallDetector::ShouldResetClientIds(PrefService* local_state) {
  // The existence of the pref indicates that it has been set when we saved the
  // MachineId and thus we need to update the member variable for this session
  // and clear the pref for future runs. We shouldn't clear the pref multiple
  // times because it may have been cloned again.
  if (!should_reset_client_ids_ &&
      local_state->HasPrefPath(prefs::kMetricsResetIds)) {
    should_reset_client_ids_ = local_state->GetBoolean(prefs::kMetricsResetIds);
    local_state->ClearPref(prefs::kMetricsResetIds);
  }

  return should_reset_client_ids_;
}

bool ClonedInstallDetector::ClonedInstallDetectedInCurrentSession() const {
  return detected_this_session_;
}

base::CallbackListSubscription
ClonedInstallDetector::AddOnClonedInstallDetectedCallback(
    base::OnceClosure callback) {
  if (detected_this_session_) {
    // If this install has already been detected as cloned, run the callback
    // immediately.
    std::move(callback).Run();
    return base::CallbackListSubscription();
  }
  return callback_list_.Add(std::move(callback));
}

void ClonedInstallDetector::SaveMachineIdForTesting(PrefService* local_state,
                                                    const std::string& raw_id) {
  SaveMachineId(local_state, raw_id);
}

// static
void ClonedInstallDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMetricsResetIds, false);
  registry->RegisterIntegerPref(prefs::kMetricsMachineId, 0);
  registry->RegisterIntegerPref(prefs::kClonedResetCount, 0);
  registry->RegisterInt64Pref(prefs::kFirstClonedResetTimestamp, 0);
  registry->RegisterInt64Pref(prefs::kLastClonedResetTimestamp, 0);
}

ClonedInstallInfo ClonedInstallDetector::ReadClonedInstallInfo(
    PrefService* local_state) {
  return ClonedInstallInfo{
      .last_reset_timestamp =
          local_state->GetInt64(prefs::kLastClonedResetTimestamp),
      .first_reset_timestamp =
          local_state->GetInt64(prefs::kFirstClonedResetTimestamp),
      .reset_count = local_state->GetInteger(prefs::kClonedResetCount)};
}

void ClonedInstallDetector::ClearClonedInstallInfo(PrefService* local_state) {
  local_state->ClearPref(prefs::kClonedResetCount);
  local_state->ClearPref(prefs::kFirstClonedResetTimestamp);
  local_state->ClearPref(prefs::kLastClonedResetTimestamp);
}

void ClonedInstallDetector::RecordClonedInstallInfo(PrefService* local_state) {
  ClonedInstallInfo cloned = ReadClonedInstallInfo(local_state);

  // Make sure that at the first time of reset, the first_timestamp matches with
  // the last_timestamp.
  int64_t time = base::Time::Now().ToTimeT();

  // Only set |prefs::kFirstClonedResetTimestamp| when the client needs to be
  // reset due to cloned install for the first time.
  if (cloned.reset_count == 0) {
    local_state->SetInt64(prefs::kFirstClonedResetTimestamp, time);
  }
  local_state->SetInt64(prefs::kLastClonedResetTimestamp, time);
  local_state->SetInteger(prefs::kClonedResetCount, cloned.reset_count + 1);
}

}  // namespace metrics
