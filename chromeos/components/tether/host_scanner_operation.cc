// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner_operation.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/connection_preserver.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"

namespace chromeos {

namespace tether {

namespace {

multidevice::RemoteDeviceRefList PrioritizeDevices(
    const multidevice::RemoteDeviceRefList& devices_to_connect,
    HostScanDevicePrioritizer* host_scan_device_prioritizer) {
  multidevice::RemoteDeviceRefList mutable_devices_to_connect =
      devices_to_connect;
  host_scan_device_prioritizer->SortByHostScanOrder(
      &mutable_devices_to_connect);
  return mutable_devices_to_connect;
}

bool IsTetheringAvailableWithValidDeviceStatus(
    const TetherAvailabilityResponse* response) {
  if (!response)
    return false;

  if (!response->has_device_status())
    return false;

  if (!response->has_response_code())
    return false;

  const TetherAvailabilityResponse_ResponseCode response_code =
      response->response_code();
  if (response_code ==
          TetherAvailabilityResponse_ResponseCode::
              TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED ||
      response_code ==
          TetherAvailabilityResponse_ResponseCode::
              TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE ||
      response_code ==
          TetherAvailabilityResponse_ResponseCode::
              TetherAvailabilityResponse_ResponseCode_LAST_PROVISIONING_FAILED) {
    return true;
  }

  return false;
}

bool AreGmsCoreNotificationsDisabled(
    const TetherAvailabilityResponse* response) {
  if (!response)
    return false;

  if (!response->has_response_code())
    return false;

  return response->response_code() ==
             TetherAvailabilityResponse_ResponseCode::
                 TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_LEGACY ||
         response->response_code() ==
             TetherAvailabilityResponse_ResponseCode::
                 TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_WITH_NOTIFICATION_CHANNEL;
}

}  // namespace

// static
HostScannerOperation::Factory*
    HostScannerOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<HostScannerOperation>
HostScannerOperation::Factory::NewInstance(
    const multidevice::RemoteDeviceRefList& devices_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder,
    ConnectionPreserver* connection_preserver) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(
      devices_to_connect, device_sync_client, secure_channel_client,
      host_scan_device_prioritizer, tether_host_response_recorder,
      connection_preserver);
}

// static
void HostScannerOperation::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<HostScannerOperation>
HostScannerOperation::Factory::BuildInstance(
    const multidevice::RemoteDeviceRefList& devices_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder,
    ConnectionPreserver* connection_preserver) {
  return base::WrapUnique(new HostScannerOperation(
      devices_to_connect, device_sync_client, secure_channel_client,
      host_scan_device_prioritizer, tether_host_response_recorder,
      connection_preserver));
}

HostScannerOperation::ScannedDeviceInfo::ScannedDeviceInfo(
    multidevice::RemoteDeviceRef remote_device,
    const DeviceStatus& device_status,
    bool setup_required)
    : remote_device(remote_device),
      device_status(device_status),
      setup_required(setup_required) {}

HostScannerOperation::ScannedDeviceInfo::~ScannedDeviceInfo() = default;

bool operator==(const HostScannerOperation::ScannedDeviceInfo& first,
                const HostScannerOperation::ScannedDeviceInfo& second) {
  return first.remote_device == second.remote_device &&
         first.device_status.SerializeAsString() ==
             second.device_status.SerializeAsString() &&
         first.setup_required == second.setup_required;
}

HostScannerOperation::HostScannerOperation(
    const multidevice::RemoteDeviceRefList& devices_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder,
    ConnectionPreserver* connection_preserver)
    : MessageTransferOperation(
          PrioritizeDevices(devices_to_connect, host_scan_device_prioritizer),
          secure_channel::ConnectionPriority::kLow,
          device_sync_client,
          secure_channel_client),
      tether_host_response_recorder_(tether_host_response_recorder),
      connection_preserver_(connection_preserver),
      clock_(base::DefaultClock::GetInstance()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

HostScannerOperation::~HostScannerOperation() = default;

void HostScannerOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostScannerOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostScannerOperation::NotifyObserversOfScannedDeviceList(
    bool is_final_scan_result) {
  for (auto& observer : observer_list_) {
    observer.OnTetherAvailabilityResponse(
        scanned_device_list_so_far_, gms_core_notifications_disabled_devices_,
        is_final_scan_result);
  }
}

void HostScannerOperation::OnDeviceAuthenticated(
    multidevice::RemoteDeviceRef remote_device) {
  DCHECK(
      !base::Contains(device_id_to_tether_availability_request_start_time_map_,
                      remote_device.GetDeviceId()));
  device_id_to_tether_availability_request_start_time_map_[remote_device
                                                               .GetDeviceId()] =
      clock_->Now();
  SendMessageToDevice(remote_device, std::make_unique<MessageWrapper>(
                                         TetherAvailabilityRequest()));
}

void HostScannerOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper,
    multidevice::RemoteDeviceRef remote_device) {
  if (message_wrapper->GetMessageType() !=
      MessageType::TETHER_AVAILABILITY_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  TetherAvailabilityResponse* response =
      static_cast<TetherAvailabilityResponse*>(
          message_wrapper->GetProto().get());
  if (AreGmsCoreNotificationsDisabled(response)) {
    PA_LOG(WARNING)
        << "Received TetherAvailabilityResponse from device with ID "
        << remote_device.GetTruncatedDeviceIdForLogs() << " which "
        << "indicates that Google Play Services notifications are "
        << "disabled. Response code: " << response->response_code();
    gms_core_notifications_disabled_devices_.push_back(remote_device);
    NotifyObserversOfScannedDeviceList(false /* is_final_scan_result */);
  } else if (!IsTetheringAvailableWithValidDeviceStatus(response)) {
    // If the received message is invalid or if it states that tethering is
    // unavailable, ignore it.
    PA_LOG(WARNING)
        << "Received TetherAvailabilityResponse from device with ID "
        << remote_device.GetTruncatedDeviceIdForLogs() << " which "
        << "indicates that tethering is not available. Response code: "
        << response->response_code();
  } else {
    bool setup_required =
        response->response_code() ==
        TetherAvailabilityResponse_ResponseCode::
            TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED;

    PA_LOG(VERBOSE)
        << "Received TetherAvailabilityResponse from device with ID "
        << remote_device.GetTruncatedDeviceIdForLogs() << " which "
        << "indicates that tethering is available. setup_required = "
        << setup_required;

    tether_host_response_recorder_->RecordSuccessfulTetherAvailabilityResponse(
        remote_device);

    // Only attempt to preserve the BLE connection to this device if the
    // response indicated that the device can serve as a host.
    connection_preserver_->HandleSuccessfulTetherAvailabilityResponse(
        remote_device.GetDeviceId());

    scanned_device_list_so_far_.push_back(ScannedDeviceInfo(
        remote_device, response->device_status(), setup_required));
    NotifyObserversOfScannedDeviceList(false /* is_final_scan_result */);
  }

  RecordTetherAvailabilityResponseDuration(remote_device.GetDeviceId());

  // Unregister the device after a TetherAvailabilityResponse has been received.
  // Delay this in order to let |connection_preserver_| fully preserve the
  // connection, if necessary, before attempting to tear down the connection.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HostScannerOperation::UnregisterDevice,
                                weak_ptr_factory_.GetWeakPtr(), remote_device));
}

void HostScannerOperation::OnOperationStarted() {
  DCHECK(scanned_device_list_so_far_.empty());

  // Send out an empty device list scan to let observers know that the operation
  // has begun.
  NotifyObserversOfScannedDeviceList(false /* is_final_scan_result */);
}

void HostScannerOperation::OnOperationFinished() {
  NotifyObserversOfScannedDeviceList(true /* is_final_scan_result */);
}

MessageType HostScannerOperation::GetMessageTypeForConnection() {
  return MessageType::TETHER_AVAILABILITY_REQUEST;
}

void HostScannerOperation::SetTestDoubles(
    base::Clock* clock_for_test,
    scoped_refptr<base::TaskRunner> test_task_runner) {
  clock_ = clock_for_test;
  task_runner_ = test_task_runner;
}

void HostScannerOperation::RecordTetherAvailabilityResponseDuration(
    const std::string device_id) {
  if (!base::Contains(device_id_to_tether_availability_request_start_time_map_,
                      device_id) ||
      device_id_to_tether_availability_request_start_time_map_[device_id]
          .is_null()) {
    LOG(ERROR) << "Failed to record TetherAvailabilityResponse duration: "
               << "start time is invalid";
    return;
  }

  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.TetherAvailabilityResponseDuration",
      clock_->Now() -
          device_id_to_tether_availability_request_start_time_map_[device_id]);
  device_id_to_tether_availability_request_start_time_map_.erase(device_id);
}

}  // namespace tether

}  // namespace chromeos
