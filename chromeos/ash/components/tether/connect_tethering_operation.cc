// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/connect_tethering_operation.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash::tether {

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
    const TetherHost& tether_host,
    raw_ptr<HostConnection::Factory> host_connection_factory,
    bool setup_required) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        tether_host, host_connection_factory, setup_required);
  }

  return base::WrapUnique(new ConnectTetheringOperation(
      tether_host, host_connection_factory, setup_required));
}

// static
void ConnectTetheringOperation::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

ConnectTetheringOperation::Factory::~Factory() = default;

ConnectTetheringOperation::ConnectTetheringOperation(
    const TetherHost& tether_host,
    raw_ptr<HostConnection::Factory> host_connection_factory,
    bool setup_required)
    : MessageTransferOperation(
          tether_host,
          HostConnection::Factory::ConnectionPriority::kHigh,
          host_connection_factory),
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

void ConnectTetheringOperation::OnDeviceAuthenticated() {
  connect_tethering_request_start_time_ = clock_->Now();
  SendMessage(std::make_unique<MessageWrapper>(ConnectTetheringRequest()),
              base::BindOnce(
                  &ConnectTetheringOperation::NotifyConnectTetheringRequestSent,
                  weak_ptr_factory_.GetWeakPtr()));
}

void ConnectTetheringOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper) {
  if (message_wrapper->GetMessageType() !=
      MessageType::CONNECT_TETHERING_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  ConnectTetheringResponse* response =
      static_cast<ConnectTetheringResponse*>(message_wrapper->GetProto());
  if (response->response_code() ==
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS) {
    if (response->has_ssid() && response->has_password()) {
      PA_LOG(VERBOSE)
          << "Received ConnectTetheringResponse from device with ID "
          << GetDeviceId(/*truncate_for_logs=*/true) << " and "
          << "response_code == SUCCESS. Config: {ssid: \"" << response->ssid()
          << "\", password: \"" << response->password() << "\"}";

      // Save the response values here, but do not notify observers until
      // OnOperationFinished(). Notifying observers at this point can cause this
      // object to be deleted, resulting in a crash.
      ssid_to_return_ = response->ssid();
      password_to_return_ = response->password();
    } else {
      PA_LOG(ERROR) << "Received ConnectTetheringResponse from device with ID "
                    << GetDeviceId(/*truncate_for_logs=*/true) << " and "
                    << "response_code == SUCCESS, but the response did not "
                    << "contain a Wi-Fi SSID and/or password.";
      error_code_to_return_ =
          HostResponseErrorCode::INVALID_HOTSPOT_CREDENTIALS;
    }
  } else {
    PA_LOG(WARNING)
        << "Received failing ConnectTetheringResponse from device with ID "
        << GetDeviceId(/*truncate_for_logs=*/true) << " and "
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
  StopOperation();
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

void ConnectTetheringOperation::NotifyConnectTetheringRequestSent() {
  for (auto& observer : observer_list_) {
    observer.OnConnectTetheringRequestSent();
  }
}

void ConnectTetheringOperation::NotifyObserversOfSuccessfulResponse(
    const std::string& ssid,
    const std::string& password) {
  for (auto& observer : observer_list_) {
    observer.OnSuccessfulConnectTetheringResponse(ssid, password);
  }
}

void ConnectTetheringOperation::NotifyObserversOfConnectionFailure(
    HostResponseErrorCode error_code) {
  for (auto& observer : observer_list_) {
    observer.OnConnectTetheringFailure(error_code);
  }
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
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG:
      return HostResponseErrorCode::INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_INVALID_NEW_SOFT_AP_CONFIG:
      return HostResponseErrorCode::INVALID_NEW_SOFT_AP_CONFIG;
    case ConnectTetheringResponse_ResponseCode::
        ConnectTetheringResponse_ResponseCode_INVALID_WIFI_AP_CONFIG:
      return HostResponseErrorCode::INVALID_WIFI_AP_CONFIG;
    default:
      break;
  }

  return HostResponseErrorCode::UNRECOGNIZED_RESPONSE_ERROR;
}

void ConnectTetheringOperation::SetClockForTest(base::Clock* clock_for_test) {
  clock_ = clock_for_test;
}

}  // namespace ash::tether
