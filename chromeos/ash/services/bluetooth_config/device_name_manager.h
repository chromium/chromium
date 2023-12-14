// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/observer_list.h"

class PrefService;

namespace ash::bluetooth_config {

// Manages saving and retrieving nicknames for Bluetooth devices. This nickname
// is local to only the device and is visible to all users of the device.
class DeviceNameManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when the nickname of device with id |device_id| has changed to
    // |nickname|. If |nickname| is null, the nickname has been removed for
    // |device_id|.
    virtual void OnDeviceNicknameChanged(
        const std::string& device_id,
        const std::optional<std::string>& nickname) = 0;
  };

  // The pref name used for the map of Bluetooth device IDs to nicknames.
  static const char kDeviceIdToNicknameMapPrefName[];

  // The pref name used for the legacy map of Bluetooth device IDs to nicknames.
  static const char kDeviceIdToNicknameMapPrefNameLegacy[];

  virtual ~DeviceNameManager();

  // Retrieves the nickname of the Bluetooth device with ID |device_id| or
  // abs::nullopt if not found.
  virtual std::optional<std::string> GetDeviceNickname(
      const std::string& device_id) = 0;

  // Sets the nickname of the Bluetooth device with ID |device_id| for all users
  // of the current device, if |nickname| is valid.
  virtual void SetDeviceNickname(const std::string& device_id,
                                 const std::string& nickname) = 0;

  // Removes the nickname of the Bluetooth device with ID |device_id| for all
  // users of the current device.
  virtual void RemoveDeviceNickname(const std::string& device_id) = 0;

  // Sets the PrefService used to store nicknames.
  virtual void SetPrefs(PrefService* local_state) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  DeviceNameManager();

  void NotifyDeviceNicknameChanged(const std::string& device_id,
                                   const std::optional<std::string>& nickname);

  base::ObserverList<Observer> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_H_
