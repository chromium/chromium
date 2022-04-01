// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FWUPD_FWUPD_CLIENT_H_
#define CHROMEOS_DBUS_FWUPD_FWUPD_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/observer_list.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/fwupd/fwupd_device.h"
#include "chromeos/dbus/fwupd/fwupd_properties.h"
#include "chromeos/dbus/fwupd/fwupd_update.h"

namespace chromeos {
using FirmwareInstallOptions = std::map<std::string, bool>;

// FwupdClient is used for handling signals from the fwupd daemon.
class COMPONENT_EXPORT(CHROMEOS_DBUS_FWUPD) FwupdClient : public DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnDeviceListResponse(FwupdDeviceList* devices) = 0;
    virtual void OnUpdateListResponse(const std::string& device_id,
                                      FwupdUpdateList* updates) = 0;
    virtual void OnInstallResponse(bool success) = 0;
    virtual void OnPropertiesChangedResponse(FwupdProperties* properties) = 0;
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

  // Used to call the protected initialization in unit tests.
  void InitForTesting(dbus::Bus* bus) { Init(bus); }

  void SetPropertiesForTesting(uint32_t percentage, uint32_t status) {
    properties_->percentage.ReplaceValue(percentage);
    properties_->status.ReplaceValue(status);
  }

  // Query fwupd for updates that are available for a particular device.
  virtual void RequestUpdates(const std::string& device_id) = 0;

  // Query fwupd for devices that are currently connected.
  virtual void RequestDevices() = 0;

  virtual void InstallUpdate(const std::string& device_id,
                             base::ScopedFD file_descriptor,
                             FirmwareInstallOptions options) = 0;

 protected:
  friend class FwupdClientTest;

  // Auxiliary variables for testing.
  // TODO(swifton): Replace this with an observer.
  bool client_is_in_testing_mode_ = false;
  int device_signal_call_count_for_testing_ = 0;

  // Holds the Fwupd Dbus properties for percentage and status.
  std::unique_ptr<FwupdProperties> properties_;

  base::ObserverList<Observer> observers_;
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FWUPD_FWUPD_CLIENT_H_
