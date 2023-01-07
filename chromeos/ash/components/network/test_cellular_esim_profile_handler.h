// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "dbus/object_path.h"

namespace ash {

// A Test implementation of CellularESimProfileHandler that stores profile list
// in-memory and fetches esim profiles directly from the fake hermes clients.
class TestCellularESimProfileHandler : public CellularESimProfileHandler {
 public:
  TestCellularESimProfileHandler();
  ~TestCellularESimProfileHandler() override;

  void SetHasRefreshedProfilesForEuicc(const std::string& eid,
                                       const dbus::ObjectPath& euicc_path,
                                       bool has_refreshed);

  // Enables or disables profile list update notification.
  // When set to false, this class will disable triggering the
  // NotifyESimProfileListUpdated() and when the next time it's set to true, it
  // will call the NotifyESimProfileListUpdated() to fire any pending list
  // update notification.
  void SetEnableNotifyProfileListUpdate(bool enable_notify_profile_list_update);

  // CellularESimProfileHandler:
  std::vector<CellularESimProfile> GetESimProfiles() override;
  bool HasRefreshedProfilesForEuicc(const std::string& eid) override;
  bool HasRefreshedProfilesForEuicc(
      const dbus::ObjectPath& euicc_path) override;
  void SetDevicePrefs(PrefService* device_prefs) override;
  void OnHermesPropertiesUpdated() override;

 private:
  std::vector<CellularESimProfile> esim_profile_states_;
  base::flat_set<std::string> refreshed_eids_;
  base::flat_set<dbus::ObjectPath> refreshed_euicc_paths_;
  bool enable_notify_profile_list_update_;
  bool has_pending_notify_list_update_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TEST_CELLULAR_ESIM_PROFILE_HANDLER_H_
