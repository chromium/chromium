// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FWUPD_FWUPD_CLIENT_H_
#define CHROMEOS_DBUS_FWUPD_FWUPD_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/fwupd/fwupd_device.h"
#include "dbus/message.h"

namespace chromeos {
// FwupdClient is used for handling signals from the fwupd daemon.
class COMPONENT_EXPORT(CHROMEOS_DBUS_FUWPD) FwupdClient : public DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnDeviceListResponse(FwupdDeviceList* devices) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Create() should be used instead.
  FwupdClient();
  FwupdClient(const FwupdClient&) = delete;
  FwupdClient& operator=(const FwupdClient&) = delete;
  ~FwupdClient() override;

  // Factory function.
  static std::unique_ptr<FwupdClient> Create();

  // Returns the global instance if initialized. May return null.
  static FwupdClient* Get();

  // Query fwupd for upgrades that are available for a particular device.
  virtual void RequestUpgrades(std::string device_id) = 0;

  // Query fwupd for devices that are currently connected.
  virtual void RequestDevices() = 0;

 protected:
  friend class FwupdClientTest;

  // Auxiliary variables for testing.
  // TODO(swifton): Replace this with an observer.
  bool client_is_in_testing_mode_ = false;
  int device_signal_call_count_for_testing_ = 0;
  int request_upgrades_callback_call_count_for_testing_ = 0;

  base::ObserverList<Observer> observers_;
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FWUPD_FWUPD_CLIENT_H_
