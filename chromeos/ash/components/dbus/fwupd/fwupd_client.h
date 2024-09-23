// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties_dbus.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_request.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_update.h"
#include "chromeos/dbus/common/dbus_client.h"

// Enum from ash/webui/firmware_update_ui/firmware_update.mojom mirrored here
// to avoid an illegal include from ash/webui.
enum DeviceRequestId {
  kDoNotPowerOff,
  kReplugInstall,
  kInsertUSBCable,
  kRemoveUSBCable,
  kPressUnlock,
  kRemoveReplug,
  kReplugPower,
};

namespace ash {

// All values returnable by fwupd dbus signal
// The errors are consistent with
// https://fwupd.github.io/libfwupd/error.Error.html
enum class FwupdDbusResult {
  kSuccess,
  kInternalError,
  kVersionNewerError,
  kVersionSameError,
  kAlreadyPendingError,
  kAuthFailedError,
  kReadError,
  kWriteError,
  kInvalidFileError,
  kNotFoundError,
  kNothingToDoError,
  kNotSupportedError,
  kSignatureInvalidError,
  kAcPowerRequiredError,
  kPermissionDeniedError,
  kBrokenSystemError,
  kBatteryLevelTooLowError,
  kNeedsUserActionError,
  kAuthExpiredError,
  kUnknownError,
  kMaxValue = kUnknownError,
};

using FirmwareInstallOptions = std::map<std::string, bool>;
using FwupdStringToRequestIdMap = std::map<std::string, DeviceRequestId>;

class FakeFwupdClient;

// FwupdClient is used for handling signals from the fwupd daemon.
class COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdClient
    : public chromeos::DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnDeviceListResponse(FwupdDeviceList* devices) = 0;
    virtual void OnUpdateListResponse(const std::string& device_id,
                                      FwupdUpdateList* updates) = 0;
    virtual void OnPropertiesChangedResponse(FwupdProperties* properties) = 0;
    virtual void OnDeviceRequestResponse(FwupdRequest request) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  FwupdClient(const FwupdClient&) = delete;
  FwupdClient& operator=(const FwupdClient&) = delete;

  // Returns the global instance if initialized. May return null.
  static FwupdClient* Get();

  // Returns the global fake instance if initialized. May return null.
  static FakeFwupdClient* GetFake();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  void SetPropertiesForTesting(uint32_t percentage, uint32_t status) {
    properties_->SetPercentage(percentage);
    properties_->SetStatus(status);
  }

  // Query fwupd for updates that are available for a particular device.
  virtual void RequestUpdates(const std::string& device_id) = 0;

  // Query fwupd for devices that are currently connected.
  virtual void RequestDevices() = 0;

  // Install an update for |device_id|. Invokes |callback| when the operation
  // completes.
  virtual void InstallUpdate(
      const std::string& device_id,
      base::ScopedFD file_descriptor,
      FirmwareInstallOptions options,
      base::OnceCallback<void(FwupdDbusResult)> callback) = 0;

  // Updates metadata for |remote_id| with given old and new file descriptors.
  virtual void UpdateMetadata(
      const std::string& remote_id,
      base::ScopedFD data_file_descriptor,
      base::ScopedFD sig_file_descriptor,
      base::OnceCallback<void(FwupdDbusResult)> callback) = 0;

 protected:
  friend class FwupdClientTest;

  // Initialize() should be used instead.
  FwupdClient();
  ~FwupdClient() override;

  // Set fwupd client-capability feature flags.
  // See https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h for the
  // full list of FwupdFeatureFlags.
  virtual void SetFwupdFeatureFlags() = 0;

  // Auxiliary variables for testing.
  // TODO(swifton): Replace this with an observer.
  bool client_is_in_testing_mode_ = false;
  int device_signal_call_count_for_testing_ = 0;

  // Holds the Fwupd Dbus properties for percentage and status.
  std::unique_ptr<FwupdDbusProperties> properties_;

  base::ObserverList<Observer> observers_;
};
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_CLIENT_H_
