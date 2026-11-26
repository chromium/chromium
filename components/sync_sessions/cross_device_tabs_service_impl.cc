// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/cross_device_tabs_service_impl.h"

#include "base/notimplemented.h"

namespace sync_sessions {

CrossDeviceTabsServiceImpl::CrossDeviceTabsServiceImpl() = default;

CrossDeviceTabsServiceImpl::~CrossDeviceTabsServiceImpl() = default;

CrossDeviceTabsService::Snapshot CrossDeviceTabsServiceImpl::GetSnapshot()
    const {
  // TODO(crbug.com/570417844): Implement snapshot resolution.
  NOTIMPLEMENTED();
  return {};
}

std::vector<CrossDeviceTabsService::RemoteDeviceVisibilityEntry>
CrossDeviceTabsServiceImpl::GetAllRemoteDevices() const {
  // TODO(crbug.com/572717502): Implement device visibility listing.
  NOTIMPLEMENTED();
  return {};
}

void CrossDeviceTabsServiceImpl::SetDeviceVisibility(
    std::string_view cache_guid,
    SessionVisibility visibility) {
  // TODO(crbug.com/572717502): Implement visibility updates.
  NOTIMPLEMENTED();
}

void CrossDeviceTabsServiceImpl::AddObserver(Observer* observer) {
  // TODO(crbug.com/570417844): Implement upstream observation.
  NOTIMPLEMENTED();
}

void CrossDeviceTabsServiceImpl::RemoveObserver(Observer* observer) {
  // TODO(crbug.com/570417844): Implement upstream observation.
  NOTIMPLEMENTED();
}

void CrossDeviceTabsServiceImpl::Shutdown() {}

}  // namespace sync_sessions
