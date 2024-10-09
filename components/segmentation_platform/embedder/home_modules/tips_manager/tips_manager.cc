// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_manager/tips_manager.h"

#include <vector>

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"

namespace segmentation_platform {

namespace {

// Removes signal data from `pref_service` that are not defined in
// `signals::kProfileSignalNames` or `signals::kLocalSignalNames`.
// This prevents storing data for unused signals.
void PruneSignalHistory(PrefService* pref_service) {
  ScopedDictPrefUpdate update(pref_service, kTipsSignalHistory);

  std::vector<std::string> signals_to_remove;

  for (auto signal : *update) {
    if (!tips_manager::signals::kLocalSignalNames.contains(signal.first) &&
        !tips_manager::signals::kProfileSignalNames.contains(signal.first)) {
      signals_to_remove.push_back(signal.first);
    }
  }

  for (const std::string& signal_to_remove : signals_to_remove) {
    update->Remove(signal_to_remove);
  }
}

}  // namespace

TipsManager::TipsManager(PrefService* profile_pref_service,
                         PrefService* local_pref_service)
    : profile_pref_service_(profile_pref_service),
      local_pref_service_(local_pref_service) {
  CHECK(profile_pref_service_);
  CHECK(local_pref_service_);

  // Remove data for signals that are no longer used by the Tips Manager.
  // This ensures the stored signal history remains relevant and doesn't
  // contain outdated information.
  PruneSignalHistory(profile_pref_service);
  PruneSignalHistory(local_pref_service);
}

TipsManager::~TipsManager() = default;

void TipsManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kTipsSignalHistory,
                                   PrefRegistry::LOSSY_PREF);
}

void TipsManager::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTipsSignalHistory,
                                   PrefRegistry::LOSSY_PREF);
}

void TipsManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  profile_pref_service_ = nullptr;
  local_pref_service_ = nullptr;
}

bool TipsManager::NotifySignal(std::string_view signal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // A signal cannot be registered as both a local and profile-based signal.
  CHECK(!tips_manager::signals::kLocalSignalNames.contains(signal) ||
        !tips_manager::signals::kProfileSignalNames.contains(signal));

  if (tips_manager::signals::kLocalSignalNames.contains(signal)) {
    RecordSignalToPref(signal, local_pref_service_);

    return true;
  }

  if (tips_manager::signals::kProfileSignalNames.contains(signal)) {
    RecordSignalToPref(signal, profile_pref_service_);

    return true;
  }

  return false;
}

bool TipsManager::WasSignalFired(std::string_view signal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Determine the correct pref service to use based on the `signal`.
  PrefService* pref_service =
      tips_manager::signals::kLocalSignalNames.contains(signal)
          ? local_pref_service_
          : profile_pref_service_;

  const base::Value::Dict& signal_history =
      pref_service->GetDict(kTipsSignalHistory);

  return signal_history.contains(signal);
}

bool TipsManager::WasSignalFiredWithin(std::string_view signal,
                                       base::TimeDelta window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Determine the correct pref service to use based on the `signal`.
  PrefService* pref_service =
      tips_manager::signals::kLocalSignalNames.contains(signal)
          ? local_pref_service_
          : profile_pref_service_;

  const base::Value::Dict& signal_history =
      pref_service->GetDict(kTipsSignalHistory);

  const base::Value::Dict* signal_data = signal_history.FindDict(signal);

  if (!signal_data) {
    return false;  // Signal not found in history.
  }

  std::optional<base::Time> last_observed_time =
      base::ValueToTime(signal_data->Find(kLastObservedTime));

  if (!last_observed_time.has_value()) {
    return false;  // Invalid or missing timestamp.
  }

  return (base::Time::Now() - last_observed_time.value()) <= window;
}

bool TipsManager::RecordSignalToPref(std::string_view signal,
                                     PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pref_service);

  ScopedDictPrefUpdate update(pref_service, kTipsSignalHistory);

  base::Value::Dict* signal_history = update->EnsureDict(signal);

  if (signal_history->empty()) {
    signal_history->Set(kFirstObservedTime,
                        base::TimeToValue(base::Time::Now()));

    signal_history->Set(kLastObservedTime,
                        base::TimeToValue(base::Time::Now()));

    signal_history->Set(kTotalOccurrences, 1);

    return true;
  }

  std::optional<int> occurrences = signal_history->FindInt(kTotalOccurrences);

  if (!occurrences.has_value()) {
    // This indicates potential data corruption in the preferences for `signal`.
    return false;
  }

  signal_history->Set(kLastObservedTime, base::TimeToValue(base::Time::Now()));

  signal_history->Set(kTotalOccurrences, occurrences.value() + 1);

  return true;
}

}  // namespace segmentation_platform
