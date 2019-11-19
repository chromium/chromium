// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cloned_install_detector.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
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

ClonedInstallDetector::ClonedInstallDetector() {}

ClonedInstallDetector::~ClonedInstallDetector() {
}

void ClonedInstallDetector::CheckForClonedInstall(PrefService* local_state) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&MachineIdProvider::GetMachineId),
      base::Bind(&ClonedInstallDetector::SaveMachineId,
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
      id_state = ID_CHANGED;
      // TODO(jwd): Use a callback to set the reset pref. That way
      // ClonedInstallDetector doesn't need to know about this pref.
      local_state->SetBoolean(prefs::kMetricsResetIds, true);
    } else {
      id_state = ID_UNCHANGED;
    }
  }

  LogMachineIdState(id_state);

  local_state->SetInteger(prefs::kMetricsMachineId, hashed_id);
}

// static
void ClonedInstallDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kMetricsMachineId, 0);
}

}  // namespace metrics
