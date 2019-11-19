// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_OPERATION_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/tether/message_transfer_operation.h"

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace tether {

class ConnectionPreserver;
class HostScanDevicePrioritizer;
class MessageWrapper;
class TetherHostResponseRecorder;

// Operation used to perform a host scan. Attempts to connect to each of the
// devices passed and sends a TetherAvailabilityRequest to each connected device
// once an authenticated channel has been established; once a response has been
// received, HostScannerOperation alerts observers of devices which can provide
// a tethering connection.
class HostScannerOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<HostScannerOperation> NewInstance(
        const multidevice::RemoteDeviceRefList& devices_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        HostScanDevicePrioritizer* host_scan_device_prioritizer,
        TetherHostResponseRecorder* tether_host_response_recorder,
        ConnectionPreserver* connection_preserver);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<HostScannerOperation> BuildInstance(
        const multidevice::RemoteDeviceRefList& devices_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        HostScanDevicePrioritizer* host_scan_device_prioritizer,
        TetherHostResponseRecorder* tether_host_response_recorder,
        ConnectionPreserver* connection_preserver);

   private:
    static Factory* factory_instance_;
  };

  struct ScannedDeviceInfo {
    ScannedDeviceInfo(multidevice::RemoteDeviceRef remote_device,
                      const DeviceStatus& device_status,
                      bool setup_required);
    ~ScannedDeviceInfo();

    friend bool operator==(const ScannedDeviceInfo& first,
                           const ScannedDeviceInfo& second);

    multidevice::RemoteDeviceRef remote_device;
    DeviceStatus device_status;
    bool setup_required;
  };

  class Observer {
   public:
    // Invoked once with an empty list when the operation begins, then invoked
    // repeatedly once each result comes in. After all devices have been
    // processed, the callback is invoked one final time with
    // |is_final_scan_result| = true.
    virtual void OnTetherAvailabilityResponse(
        const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
        const multidevice::RemoteDeviceRefList&
            gms_core_notifications_disabled_devices,
        bool is_final_scan_result) = 0;
  };

  ~HostScannerOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  HostScannerOperation(
      const multidevice::RemoteDeviceRefList& devices_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      HostScanDevicePrioritizer* host_scan_device_prioritizer,
      TetherHostResponseRecorder* tether_host_response_recorder,
      ConnectionPreserver* connection_preserver);

  void NotifyObserversOfScannedDeviceList(bool is_final_scan_result);

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) override;
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         multidevice::RemoteDeviceRef remote_device) override;
  void OnOperationStarted() override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;

  std::vector<ScannedDeviceInfo> scanned_device_list_so_far_;

 private:
  friend class HostScannerOperationTest;
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest,
                           DevicesArePrioritizedDuringConstruction);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, RecordsResponseDuration);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, ErrorResponses);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, NotificationsDisabled);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, TetherAvailable);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, LastProvisioningFailed);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, SetupRequired);
  FRIEND_TEST_ALL_PREFIXES(HostScannerOperationTest, TestMultipleDevices);

  using MessageTransferOperation::UnregisterDevice;

  void SetTestDoubles(base::Clock* clock_for_test,
                      scoped_refptr<base::TaskRunner> test_task_runner);
  void RecordTetherAvailabilityResponseDuration(const std::string device_id);

  TetherHostResponseRecorder* tether_host_response_recorder_;
  ConnectionPreserver* connection_preserver_;
  base::Clock* clock_;
  scoped_refptr<base::TaskRunner> task_runner_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  multidevice::RemoteDeviceRefList gms_core_notifications_disabled_devices_;

  std::map<std::string, base::Time>
      device_id_to_tether_availability_request_start_time_map_;

  base::WeakPtrFactory<HostScannerOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostScannerOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_OPERATION_H_
