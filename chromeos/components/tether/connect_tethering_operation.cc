// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connect_tethering_operation.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"

namespace chromeos {

namespace tether {

// When setup is not required, allow a 30-second timeout. If a host device is on
// a slow data connection, enabling the tether hotspot may take a significant
// amount of time because most phones must send a "provisioning" request to
// the mobile provider to ask the provider whether tethering is allowed.
// static
const uint32_t
    ConnectTetheringOperation::kSetupNotRequiredResponseTimeoutSeconds = 30;

// When setup is required, the timeout is extended another 90 seconds because
// setup requires that the user interact with a notification on the Tether host.
// static
const uint32_t ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds =
    120;

// static
ConnectTetheringOperation::Factory*
    ConnectTetheringOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<ConnectTetheringOperation>
ConnectTetheringOperation::Factory::Create(
    multidevice::RemoteDeviceRef device_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostResponseRecorder* tether_host_response_recorder,
    bool setup_required) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        device_to_connect, device_sync_client, secure_channel_client,
        tether_host_response_recorder, setup_required);
  }

  return base::WrapUnique(new ConnectTetheringOperation(
      device_to_connect, device_sync_client, secure_channel_client,
      tether_host_response_recorder, setup_required));
}

// static
void ConnectTetheringOperation::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

ConnectTetheringOperation::Factory::~Factory() = default;

ConnectTetheringOperation::ConnectTetheringOperation(
    multidevice::RemoteDeviceRef device_to_connect,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostResponseRecorder* tether_host_response_recorder,
    bool setup_required)
    : MessageTransferOperation(
          multidevice::RemoteDeviceRefList{device_to_connect},
          secure_channel::ConnectionPriority::kHigh,
          device_sync_client,
          secure_channel_client),
      remote_device_(device_to_connect),
      tether_host_response_recorder_(tether_host_response_recorder),
      clock_(base::DefaultClock::GetInstance()),
      setup_required_(setup_required),
      error_code_to_return_(HostResponseErrorCode::NO_RESPONSE) {}

ConnectTetheringOperation::~ConnectTetheringOperation() = default;

void ConnectTetheringOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConnectTetheringOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConnectTetheringOperation::OnDeviceAuthenticated(
    multidevice::RemoteDeviceRef remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);
  connect_tethering_request_start_time_ = clock_->Now();
  connect_message_sequence_number_ = SendMessageToDevice(
      remote_device,
      std::make_unique<MessageWrapper>(ConnectTetheringRequest()));
}

void ConnectTetheringOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper,
    multidevice::RemoteDeviceRef remote_device) {
  if (message_wrapper->GetMessageType() !=
      MessageType::CONNECT_TETHERING_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  if (!(remote_device == remote_device_)) {
    // If the message came from another device, ignore it.
    return;
  }

  ConnectTetheringResponse* response =
      static_cast<ConnectTetheringResponse*>(message_wrapper->GetProto().get());
  if (response->response_code() ==
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS) {
    if (response->has_ssid() && response->has_password()) {
      PA_LOG(VERBOSE)
          << "Received ConnectTetheringResponse from device with ID "
          << remote_device.GetTruncatedDeviceIdForLogs() << " and "
          << "response_code == SUCCESS. Config: {ssid: \"" << response->ssid()
          << "\", password: \"" << response->password() << "\"}";

      tether_host_response_recorder_->RecordSuccessfulConnectTetheringResponse(
          remote_device);

      // Save the response values here, but do not notify observers until
      // OnOperationFinished(). Notifying observers at this point can cause this
      // object to be deleted, resulting in a crash.
      ssid_to_return_ = response->ssid();
      password_to_return_ = response->password();
    } else {
      PA_LOG(ERROR) << "Received ConnectTetheringResponse from device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << " and "
                    << "response_code == SUCCESS, but the response did not "
                    << "contain a Wi-Fi SSID and/or password.";
      error_code_to_return_ =
          HostResponseErrorCode::INVALID_HOTSPOT_CREDENTIALS;
    }
  } else {
    PA_LOG(WARNING)
        << "Received failing ConnectTetheringResponse from device with ID "
        << remote_device.GetTruncatedDeviceIdForLogs() << " and "
        << "response_code == " << response->response_code() << ".";
    error_code_to_return_ = ConnectTetheringResponseCodeToHostResponseErrorCode(
        response->response_code());
  }

  // UMA_HISTOGRAM_MEDIUM_TIMES is used because UMA_HISTOGRAM_TIMES has a max
  // of 10 seconds, and it can take up to 90 seconds for a
  // ConnectTetheringResponse.
  DCHECK(!connect_tethering_request_start_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "InstantTethering.Performance.ConnectTetheringResponseDuration",
      clock_->Now() - connect_tethering_request_start_time_);

  // Now that a response has been received, the device can be unregistered.
  UnregisterDevice(remote_device);
}

void ConnectTetheringOperation::OnOperationFinished() {
  // If |ssid_to_return_| has not been set, either the operation finished with a
  // failed response or no connection succeeded at all. In these cases, notify
  // observers of a failure.
  if (ssid_to_return_.empty()) {
    NotifyObserversOfConnectionFailure(error_code_to_return_);
    return;
  }

  NotifyObserversOfSuccessfulResponse(ssid_to_return_, password_to_return_);
}

MessageType ConnectTetheringOperation::GetMessageTypeForConnection() {
  return MessageType::CONNECT_TETHERING_REQUEST;
}

void ConnectTetheringOperation::OnMessageSent(int sequence_number) {
  if (sequence_number != connect_message_sequence_number_)
    return;

  NotifyConnectTetheringRequestSent();
}

void ConnectTetheringOperation::NotifyConnectTetheringRequestSent() {
  for (auto& observer : observer_list_)
    observer.OnConnectTetheringRequestSent(remote_device_);
}

void ConnectTetheringOperation::NotifyObserversOfSuccessfulResponse(
    const std::string& ssid,
    const std::string& password) {
  for (auto& observer : observer_list_) {
    observer.OnSuccessfulConnectTetheringResponse(remote_device_, ssid,
                                                  password);
  }
}

void ConnectTetheringOperation::NotifyObserversOfConnectionFailure(
    HostResponseErrorCode error_code) {
  for (auto& observer : observer_list_)
    observer.OnConnectTetheringFailure(remote_device_, error_code);
}

uint32_t ConnectTetheringOperation::GetMessageTimeoutSeconds() {
  return setup_required_
             ? ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds
             : ConnectTetheringOperation::
                   kSetupNotRequiredResponseTimeoutSeconds;
}

ConnectTetheringOperation::HostResponseErrorCode
ConnectTetheringOperation::ConnectTetheringResponseCodeToHostResponseErrorCode(
    ConnectTetheringResponse_ResponseCode error_code) {
  switch (error_code) {
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_PROVISIONING_FAILED:
      return HostResponseErrorCode::PROVISIONING_FAILED;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_TETHERING_TIMEOUT:
      return HostResponseErrorCode::TETHERING_TIMEOUT;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_TETHERING_UNSUPPORTED:
      return HostResponseErrorCode::TETHERING_UNSUPPORTED;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_NO_CELL_DATA:
      return HostResponseErrorCode::NO_CELL_DATA;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_ENABLING_HOTSPOT_FAILED:
      return HostResponseErrorCode::ENABLING_HOTSPOT_FAILED;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_ENABLING_HOTSPOT_TIMEOUT:
      return HostResponseErrorCode::ENABLING_HOTSPOT_TIMEOUT;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR:
      return HostResponseErrorCode::UNKNOWN_ERROR;
    default:
      break;
  }

  return HostResponseErrorCode::UNRECOGNIZED_RESPONSE_ERROR;
}

void ConnectTetheringOperation::SetClockForTest(base::Clock* clock_for_test) {
  clock_ = clock_for_test;
}

}  // namespace tether

}  // namespace chromeos
