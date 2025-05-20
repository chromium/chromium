// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cloned_install_detector.h"

#include <stdint.h>

#include <string>

#include "base/callback_list.h"
#include "base/check_is_test.h"
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

MachineIdProvider* g_machine_id_provider_for_testing = nullptr;

bool HasMachineId() {
  if (g_machine_id_provider_for_testing) {
    CHECK_IS_TEST();
    return g_machine_id_provider_for_testing->HasId();
  }

  return MachineIdProvider().HasId();
}

std::string GetMachineId() {
  if (g_machine_id_provider_for_testing) {
    CHECK_IS_TEST();
    return g_machine_id_provider_for_testing->GetMachineId();
  }

  return MachineIdProvider().GetMachineId();
}

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
  if (!HasMachineId()) {
    return;
  }

  base::Time check_initiated_timestamp = base::Time::Now();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetMachineId),
      base::BindOnce(&ClonedInstallDetector::SaveMachineId,
                     weak_ptr_factory_.GetWeakPtr(), local_state,
                     check_initiated_timestamp));
}

void ClonedInstallDetector::SaveMachineId(PrefService* local_state,
                                          base::Time check_initiated_timestamp,
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
      local_state->SetInt64(prefs::kSessionStartTimestampForLastClonedDetection,
                            check_initiated_timestamp.ToTimeT());

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
  SaveMachineId(local_state, base::Time::Now(), raw_id);
}

// static
void ClonedInstallDetector::SetMachineIdProviderForTesting(
    MachineIdProvider* machine_id_provider) {
  CHECK_IS_TEST();
  g_machine_id_provider_for_testing = machine_id_provider;
}

// static
void ClonedInstallDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMetricsResetIds, false);
  registry->RegisterIntegerPref(prefs::kMetricsMachineId, 0);
  registry->RegisterIntegerPref(prefs::kClonedResetCount, 0);
  registry->RegisterInt64Pref(prefs::kFirstClonedResetTimestamp, 0);
  registry->RegisterInt64Pref(prefs::kLastClonedResetTimestamp, 0);
  registry->RegisterInt64Pref(
      prefs::kSessionStartTimestampForLastClonedDetection, 0);
}

ClonedInstallInfo ClonedInstallDetector::ReadClonedInstallInfo(
    PrefService* local_state) {
  return ClonedInstallInfo{
      .last_detection_timestamp = local_state->GetInt64(
          prefs::kSessionStartTimestampForLastClonedDetection),
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
  local_state->ClearPref(prefs::kSessionStartTimestampForLastClonedDetection);
}

// static
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

  if (!cloned.last_detection_timestamp) {
    // `prefs::kSessionStartTimestampForLastClonedDetection` (which is read to
    // `last_detection_timestamp`) is expected to be set when we flag the
    // client as a clone to be reset. This timestamp might be missing,
    // especially on older milestones that did not set the timestamp yet, or if
    // the user toggled UMA opt-in during the session that detected the clone.
    // In this case, we can use the reset time as a fallback value. We might
    // then mistakenly attribute some events that happened on the current
    // (cloned) install as having taken place on the original install. That's
    // acceptable.
    local_state->SetInt64(prefs::kSessionStartTimestampForLastClonedDetection,
                          time);
  }
}

}  // namespace metrics
