// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_availability_operation.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/connection_preserver.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

namespace ash::tether {

namespace {

bool IsTetheringAvailableWithValidDeviceStatus(
    const TetherAvailabilityResponse* response) {
  if (!response) {
    return false;
  }

  if (!response->has_device_status()) {
    return false;
  }

  if (!response->has_response_code()) {
    return false;
  }

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
  if (!response) {
    return false;
  }

  if (!response->has_response_code()) {
    return false;
  }

  return response->response_code() ==
             TetherAvailabilityResponse_ResponseCode::
                 TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_LEGACY ||
         response->response_code() ==
             TetherAvailabilityResponse_ResponseCode::
                 TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED_WITH_NOTIFICATION_CHANNEL;
}

}  // namespace

TetherAvailabilityOperation::Initializer::Initializer(
    raw_ptr<HostConnection::Factory> host_connection_factory,
    raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder,
    raw_ptr<ConnectionPreserver> connection_preserver)
    : host_connection_factory_(host_connection_factory),
      tether_host_response_recorder_(tether_host_response_recorder),
      connection_preserver_(connection_preserver) {}

TetherAvailabilityOperation::Initializer::~Initializer() = default;

std::unique_ptr<TetherAvailabilityOperation>
TetherAvailabilityOperation::Initializer::Initialize(
    const TetherHost& tether_host,
    TetherAvailabilityOperation::OnTetherAvailabilityOperationFinishedCallback
        callback) {
  auto operation = std::make_unique<TetherAvailabilityOperation>(
      tether_host, std::move(callback), host_connection_factory_,
      tether_host_response_recorder_, connection_preserver_);
  operation->Initialize();
  return operation;
}

TetherAvailabilityOperation::TetherAvailabilityOperation(
    const TetherHost& tether_host,
    TetherAvailabilityOperation::OnTetherAvailabilityOperationFinishedCallback
        callback,
    raw_ptr<HostConnection::Factory> host_connection_factory,
    TetherHostResponseRecorder* tether_host_response_recorder,
    ConnectionPreserver* connection_preserver)
    : MessageTransferOperation(
          tether_host,
          HostConnection::Factory::ConnectionPriority::kLow,
          host_connection_factory),
      tether_host_(tether_host),
      tether_host_response_recorder_(tether_host_response_recorder),
      connection_preserver_(connection_preserver),
      clock_(base::DefaultClock::GetInstance()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      on_operation_finished_(std::move(callback)) {}

TetherAvailabilityOperation::~TetherAvailabilityOperation() = default;

void TetherAvailabilityOperation::OnDeviceAuthenticated() {
  CHECK(!tether_availability_request_start_time_.has_value());
  tether_availability_request_start_time_ = clock_->Now();
  PA_LOG(VERBOSE) << "Sending TetherAvailabilityRequest message to "
                  << GetDeviceId(/*truncate_for_logs=*/true) << ".";
  SendMessage(std::make_unique<MessageWrapper>(TetherAvailabilityRequest()),
              /*on_message_sent=*/base::DoNothing());
}

void TetherAvailabilityOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper) {
  if (message_wrapper->GetMessageType() !=
      MessageType::TETHER_AVAILABILITY_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  TetherAvailabilityResponse* response =
      static_cast<TetherAvailabilityResponse*>(message_wrapper->GetProto());
  if (AreGmsCoreNotificationsDisabled(response)) {
    PA_LOG(WARNING)
        << "Received TetherAvailabilityResponse from device with ID "
        << GetDeviceId(/*truncate_for_logs=*/true) << " which "
        << "indicates that Google Play Services notifications are "
        << "disabled. Response code: " << response->response_code();

    scanned_device_info_result_ = ScannedDeviceInfo(
        tether_host_.GetDeviceId(), tether_host_.GetName(), std::nullopt,
        /*setup_required=*/false, /*notifications_enabled=*/false);
  } else if (!IsTetheringAvailableWithValidDeviceStatus(response)) {
    // If the received message is invalid or if it states that tethering is
    // unavailable, ignore it.
    PA_LOG(WARNING)
        << "Received TetherAvailabilityResponse from device with ID "
        << GetDeviceId(/*truncate_for_logs=*/true) << " which "
        << "indicates that tethering is not available. Response code: "
        << response->response_code();
  } else {
    bool setup_required =
        response->response_code() ==
        TetherAvailabilityResponse_ResponseCode::
            TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED;

    PA_LOG(VERBOSE)
        << "Received TetherAvailabilityResponse from device with ID "
        << GetDeviceId(/*truncate_for_logs=*/true) << " which "
        << "indicates that tethering is available. setup_required = "
        << setup_required;

    tether_host_response_recorder_->RecordSuccessfulTetherAvailabilityResponse(
        GetDeviceId(/*truncate_for_logs=*/false));

    // Only attempt to preserve the BLE connection to this device if the
    // response indicated that the device can serve as a host.
    connection_preserver_->HandleSuccessfulTetherAvailabilityResponse(
        GetDeviceId(/*truncate_for_logs=*/false));

    scanned_device_info_result_ =
        ScannedDeviceInfo(tether_host_.GetDeviceId(), tether_host_.GetName(),
                          response->device_status(), setup_required,
                          /*notifications_enabled=*/true);
  }

  RecordTetherAvailabilityResponseDuration(
      GetDeviceId(/*truncate_for_logs=*/false));

  // Unregister the device after a TetherAvailabilityResponse has been received.
  // Delay this in order to let |connection_preserver_| fully preserve the
  // connection, if necessary, before attempting to tear down the connection.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TetherAvailabilityOperation::StopOperation,
                                weak_ptr_factory_.GetWeakPtr()));
}

void TetherAvailabilityOperation::OnOperationFinished() {
  std::move(on_operation_finished_).Run(scanned_device_info_result_);
}

MessageType TetherAvailabilityOperation::GetMessageTypeForConnection() {
  return MessageType::TETHER_AVAILABILITY_REQUEST;
}

void TetherAvailabilityOperation::SetTestDoubles(
    base::Clock* clock_for_test,
    scoped_refptr<base::TaskRunner> test_task_runner) {
  clock_ = clock_for_test;
  task_runner_ = test_task_runner;
}

void TetherAvailabilityOperation::RecordTetherAvailabilityResponseDuration(
    const std::string device_id) {
  if (!tether_availability_request_start_time_.has_value()) {
    LOG(ERROR) << "Failed to record TetherAvailabilityResponse duration: "
               << "start time is invalid";
    return;
  }

  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.TetherAvailabilityResponseDuration",
      clock_->Now() - *tether_availability_request_start_time_);
  tether_availability_request_start_time_.reset();
}

}  // namespace ash::tether
