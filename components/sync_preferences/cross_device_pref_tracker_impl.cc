// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker_impl.h"

#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace sync_preferences {

CrossDevicePrefTrackerImpl::CrossDevicePrefTrackerImpl(
    PrefService* profile_pref_service,
    PrefService* local_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service) {
  // TODO(crbug.com/441330511): Implement service initialization and state
  // management (PrefService pointers, DeviceInfoSyncService pointer, Observers,
  // etc.)
}

CrossDevicePrefTrackerImpl::~CrossDevicePrefTrackerImpl() = default;

void CrossDevicePrefTrackerImpl::AddObserver(
    CrossDevicePrefTracker::Observer* observer) {
  // TODO(crbug.com/441330511): Implement Observer management.
}

void CrossDevicePrefTrackerImpl::RemoveObserver(
    CrossDevicePrefTracker::Observer* observer) {
  // TODO(crbug.com/441330511): Implement Observer management.
}

std::vector<base::Value> CrossDevicePrefTrackerImpl::GetValues(
    const std::string& pref_name,
    const DeviceFilter& filter) const {
  // TODO(crbug.com/441330219): Implement the Query API.

  return {};
}

std::optional<base::Value> CrossDevicePrefTrackerImpl::GetMostRecentValue(
    const std::string& pref_name,
    const DeviceFilter& filter) const {
  // TODO(crbug.com/441330219): Implement the Query API.

  return std::nullopt;
}

void CrossDevicePrefTrackerImpl::Shutdown() {
  // TODO(crbug.com/441330511): Implement service initialization and state
  // management (PrefService pointers, DeviceInfoSyncService pointer, Observers,
  // etc.)
}

void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  // TODO(crbug.com/441330511): Implement service initialization and state
  // management (PrefService pointers, DeviceInfoSyncService pointer, Observers,
  // etc.)
}

}  // namespace sync_preferences
