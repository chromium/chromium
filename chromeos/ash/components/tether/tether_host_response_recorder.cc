// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

#include <memory>

#include "base/values.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace ash::tether {

// static
void TetherHostResponseRecorder::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Make both of these preferences synced between devices. This ensures that
  // if users utilize Tether networks on multiple Chromebooks, they will use the
  // same prioritization criteria.
  registry->RegisterListPref(
      prefs::kMostRecentTetherAvailablilityResponderIds,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterListPref(
      prefs::kMostRecentConnectTetheringResponderIds,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

TetherHostResponseRecorder::TetherHostResponseRecorder(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

TetherHostResponseRecorder::~TetherHostResponseRecorder() = default;

void TetherHostResponseRecorder::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TetherHostResponseRecorder::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TetherHostResponseRecorder::RecordSuccessfulTetherAvailabilityResponse(
    const std::string& device_id) {
  AddRecentResponse(device_id,
                    prefs::kMostRecentTetherAvailablilityResponderIds);
}

std::vector<std::string>
TetherHostResponseRecorder::GetPreviouslyAvailableHostIds() const {
  return GetDeviceIdsForPref(prefs::kMostRecentTetherAvailablilityResponderIds);
}

void TetherHostResponseRecorder::RecordSuccessfulConnectTetheringResponse(
    const std::string& device_id) {
  if (AddRecentResponse(device_id,
                        prefs::kMostRecentConnectTetheringResponderIds)) {
    NotifyObserversPreviouslyConnectedHostIdsChanged();
  }
}

std::vector<std::string>
TetherHostResponseRecorder::GetPreviouslyConnectedHostIds() const {
  return GetDeviceIdsForPref(prefs::kMostRecentConnectTetheringResponderIds);
}

void TetherHostResponseRecorder::
    NotifyObserversPreviouslyConnectedHostIdsChanged() {
  for (Observer& observer : observer_list_) {
    observer.OnPreviouslyConnectedHostIdsChanged();
  }
}

bool TetherHostResponseRecorder::AddRecentResponse(
    const std::string& device_id,
    const std::string& pref_name) {
  const base::Value::List& ids_list = pref_service_->GetList(pref_name);

  std::string first_device_id_in_list;
  if (!ids_list.empty() && ids_list[0].is_string())
    first_device_id_in_list = ids_list[0].GetString();

  if (device_id == first_device_id_in_list) {
    // If the device ID that is being inserted is already at the front of the
    // list, there is nothing to do.
    return false;
  }

  // Create a mutable copy of the stored IDs.
  base::Value::List updated_ids = ids_list.Clone();

  // Remove the device ID if it was already present in the list.
  base::Value device_id_value(device_id);
  updated_ids.EraseValue(device_id_value);

  // Add the device ID to the front of the queue.
  updated_ids.Insert(updated_ids.begin(), std::move(device_id_value));

  // Store the updated list back in |pref_service_|.
  pref_service_->SetList(pref_name, std::move(updated_ids));

  return true;
}

std::vector<std::string> TetherHostResponseRecorder::GetDeviceIdsForPref(
    const std::string& pref_name) const {
  std::vector<std::string> device_ids;

  const base::Value::List& ids = pref_service_->GetList(pref_name);
  for (const auto& entry : ids) {
    if (entry.is_string())
      device_ids.push_back(entry.GetString());
  }

  return device_ids;
}

}  // namespace ash::tether
