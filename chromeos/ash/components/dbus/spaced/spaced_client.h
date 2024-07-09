// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_SPACED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_SPACED_CLIENT_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/spaced/spaced.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace dbus {
class Bus;
}

namespace ash {

// A class to make DBus calls for the org.chromium.Spaced service.
class COMPONENT_EXPORT(SPACED_CLIENT) SpacedClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    using SpaceEvent = spaced::StatefulDiskSpaceUpdate;
    // Called when the space is queried periodically.
    virtual void OnSpaceUpdate(const SpaceEvent& event) = 0;
  };

  struct SpaceMaps {
    SpaceMaps(std::map<uint32_t, int64_t>&& curspaces_for_uids,
              std::map<uint32_t, int64_t>&& curspaces_for_gids,
              std::map<uint32_t, int64_t>&& curspaces_for_project_ids);
    ~SpaceMaps();
    SpaceMaps(const SpaceMaps& other);

    std::map<uint32_t, int64_t> curspaces_for_uids;
    std::map<uint32_t, int64_t> curspaces_for_gids;
    std::map<uint32_t, int64_t> curspaces_for_project_ids;
  };

  using GetSizeCallback = chromeos::DBusMethodCallback<int64_t>;
  using BoolCallback = chromeos::DBusMethodCallback<bool>;
  using GetSpacesForIdsCallback = chromeos::DBusMethodCallback<SpaceMaps>;

  SpacedClient(const SpacedClient&) = delete;
  SpacedClient& operator=(const SpacedClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static SpacedClient* Get();

  // Gets free disk space available in bytes for the given file path.
  virtual void GetFreeDiskSpace(const std::string& path,
                                GetSizeCallback callback) = 0;

  // Gets total disk space available in bytes on the current partition for the
  // given file path.
  virtual void GetTotalDiskSpace(const std::string& path,
                                 GetSizeCallback callback) = 0;

  // Gets the total disk space available in bytes for usage on the device.
  virtual void GetRootDeviceSize(GetSizeCallback callback) = 0;

  // Gets whether the user's cryptohome is mounted with quota enabled.
  virtual void IsQuotaSupported(const std::string& path,
                                BoolCallback callback) = 0;

  // Gets the current disk space used by the given uid.
  virtual void GetQuotaCurrentSpaceForUid(const std::string& path,
                                          uint32_t uid,
                                          GetSizeCallback callback) = 0;
  // Gets the current disk space used by the given gid.
  virtual void GetQuotaCurrentSpaceForGid(const std::string& path,
                                          uint32_t gid,
                                          GetSizeCallback callback) = 0;
  // Gets the current disk space used by the given project_id.
  virtual void GetQuotaCurrentSpaceForProjectId(const std::string& path,
                                                uint32_t project_id,
                                                GetSizeCallback callback) = 0;

  // Gets the current disk spaces used by each of the given UIDs, GIDs, and
  // project IDs, and returns maps from the IDs to their disk space usages.
  virtual void GetQuotaCurrentSpacesForIds(
      const std::string& path,
      const std::vector<uint32_t>& uids,
      const std::vector<uint32_t>& gids,
      const std::vector<uint32_t>& project_ids,
      GetSpacesForIdsCallback callback) = 0;

  // Adds an observer.
  void AddObserver(Observer* const observer) {
    observers_.AddObserver(observer);
  }

  // Removes an observer.
  void RemoveObserver(Observer* const observer) {
    observers_.RemoveObserver(observer);
  }

  // Returns true if the `OnStatefulDiskSpaceUpdate` signal has been connected.
  bool IsConnected() const { return connected_; }

 protected:
  // Initialize/Shutdown should be used instead.
  SpacedClient();
  virtual ~SpacedClient();

  // List of observers.
  base::ObserverList<Observer> observers_;

  // Whether the `OnStatefulDiskSpaceUpdate` signal has been connected.
  bool connected_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SPACED_SPACED_CLIENT_H_
