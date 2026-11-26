// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_CROSS_DEVICE_TABS_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_CROSS_DEVICE_TABS_SERVICE_IMPL_H_

#include <string_view>
#include <vector>

#include "components/sync_sessions/cross_device_tabs_service.h"
#include "components/sync_sessions/session_sync_prefs.h"

namespace sync_sessions {

class CrossDeviceTabsServiceImpl final : public CrossDeviceTabsService {
 public:
  CrossDeviceTabsServiceImpl();

  CrossDeviceTabsServiceImpl(const CrossDeviceTabsServiceImpl&) = delete;
  CrossDeviceTabsServiceImpl& operator=(const CrossDeviceTabsServiceImpl&) =
      delete;

  ~CrossDeviceTabsServiceImpl() override;

  // CrossDeviceTabsService:
  Snapshot GetSnapshot() const override;
  std::vector<RemoteDeviceVisibilityEntry> GetAllRemoteDevices() const override;
  void SetDeviceVisibility(std::string_view cache_guid,
                           SessionVisibility visibility) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // KeyedService:
  void Shutdown() override;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_CROSS_DEVICE_TABS_SERVICE_IMPL_H_
