// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_weave_client_connection.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/background_eid_generator.h"
#include "chromeos/services/secure_channel/wire_message.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace chromeos {

namespace secure_channel {

namespace weave {

namespace {

typedef BluetoothLowEnergyWeavePacketReceiver::State ReceiverState;

// The UUID of the TX characteristic used to transmit data to the server.
const char kTXCharacteristicUUID[] = "00000100-0004-1000-8000-001A11000101";

// The UUID of the RX characteristic used to receive data from the server.
const char kRXCharacteristicUUID[] = "00000100-0004-1000-8000-001A11000102";

// If sending a message fails, retry up to 2 additional times. This means that
// each message gets 3 attempts: the first one, and 2 retries.
const int kMaxNumberOfRetryAttempts = 2;

// Timeouts for various status types.
const int kConnectionLatencyTimeoutSeconds = 2;
const int kGattConnectionTimeoutSeconds = 15;
const int kGattCharacteristicsTimeoutSeconds = 10;
const int kNotifySessionTimeoutSeconds = 5;
const int kConnectionResponseTimeoutSeconds = 2;
const int kSendingMessageTimeoutSeconds = 5;

}  // namespace

// static
BluetoothLowEnergyWeaveClientConnection::Factory*
    BluetoothLowEnergyWeaveClientConnection::Factory::factory_instance_ =
        nullptr;

// static
std::unique_ptr<Connection>
BluetoothLowEnergyWeaveClientConnection::Factory::NewInstance(
    multidevice::RemoteDeviceRef remote_device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const device::BluetoothUUID remote_service_uuid,
    const std::string& device_address,
    bool should_set_low_connection_latency) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(remote_device, adapter,
                                          remote_service_uuid, device_address,
                                          should_set_low_connection_latency);
}

// static
void BluetoothLowEnergyWeaveClientConnection::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<Connection>
BluetoothLowEnergyWeaveClientConnection::Factory::BuildInstance(
    multidevice::RemoteDeviceRef remote_device,
    scoped_refptr<device::BluetoothAdapter> adapter,
    const device::BluetoothUUID remote_service_uuid,
    const std::string& device_address,
    bool should_set_low_connection_latency) {
  return std::make_unique<BluetoothLowEnergyWeaveClientConnection>(
      remote_device, adapter, remote_service_uuid, device_address,
      should_set_low_connection_latency);
}

// static
base::TimeDelta BluetoothLowEnergyWeaveClientConnection::GetTimeoutForSubStatus(
    SubStatus sub_status) {
  switch (sub_status) {
    case SubStatus::WAITING_CONNECTION_RESPONSE:
      return base::TimeDelta::FromSeconds(kConnectionResponseTimeoutSeconds);
    case SubStatus::WAITING_CONNECTION_LATENCY:
      return base::TimeDelta::FromSeconds(kConnectionLatencyTimeoutSeconds);
    case SubStatus::WAITING_GATT_CONNECTION:
      return base::TimeDelta::FromSeconds(kGattConnectionTimeoutSeconds);
    case SubStatus::WAITING_CHARACTERISTICS:
      return base::TimeDelta::FromSeconds(kGattCharacteristicsTimeoutSeconds);
    case SubStatus::WAITING_NOTIFY_SESSION:
      return base::TimeDelta::FromSeconds(kNotifySessionTimeoutSeconds);
    case SubStatus::CONNECTED_AND_SENDING_MESSAGE:
      return base::TimeDelta::FromSeconds(kSendingMessageTimeoutSeconds);
    default:
      // Max signifies that there should be no timeout.
      return base::TimeDelta::Max();
  }
}

// static
std::string BluetoothLowEnergyWeaveClientConnection::SubStatusToString(
    SubStatus sub_status) {
  switch (sub_status) {
    case SubStatus::DISCONNECTED:
      return "[disconnected]";
    case SubStatus::WAITING_CONNECTION_LATENCY:
      return "[waiting to set connection latency]";
    case SubStatus::WAITING_GATT_CONNECTION:
      return "[waiting for GATT connection to be created]";
    case SubStatus::WAITING_CHARACTERISTICS:
      return "[waiting for GATT characteristics to be found]";
    case SubStatus::CHARACTERISTICS_FOUND:
      return "[GATT characteristics have been found]";
    case SubStatus::WAITING_NOTIFY_SESSION:
      return "[waiting for notify session to begin]";
    case SubStatus::NOTIFY_SESSION_READY:
      return "[notify session is ready]";
    case SubStatus::WAITING_CONNECTION_RESPONSE:
      return "[waiting for \"connection response\" uWeave packet]";
    case SubStatus::CONNECTED_AND_IDLE:
      return "[connected and idle]";
    case SubStatus::CONNECTED_AND_SENDING_MESSAGE:
      return "[connected and sending message]";
    default:
      return "[invalid state]";
  }
}

BluetoothLowEnergyWeaveClientConnection::
    BluetoothLowEnergyWeaveClientConnection(
        multidevice::RemoteDeviceRef device,
        scoped_refptr<device::BluetoothAdapter> adapter,
        const device::BluetoothUUID remote_service_uuid,
        const std::string& device_address,
        bool should_set_low_connection_latency)
    : Connection(device),
      initial_device_address_(device_address),
      should_set_low_connection_latency_(should_set_low_connection_latency),
      adapter_(adapter),
      remote_service_({remote_service_uuid, std::string()}),
      packet_generator_(
          std::make_unique<BluetoothLowEnergyWeavePacketGenerator>()),
      packet_receiver_(std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          BluetoothLowEnergyWeavePacketReceiver::ReceiverType::CLIENT)),
      tx_characteristic_(
          {device::BluetoothUUID(kTXCharacteristicUUID), std::string()}),
      rx_characteristic_(
          {device::BluetoothUUID(kRXCharacteristicUUID), std::string()}),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      timer_(std::make_unique<base::OneShotTimer>()),
      sub_status_(SubStatus::DISCONNECTED) {
  DCHECK(!initial_device_address_.empty());
  adapter_->AddObserver(this);
}

BluetoothLowEnergyWeaveClientConnection::
    ~BluetoothLowEnergyWeaveClientConnection() {
  if (sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE || IsConnected()) {
    // Deleting this object without calling Disconnect() may result in the
    // connection staying active longer than intended, which can lead to errors.
    // See https://crbug.com/763604.
    PA_LOG(WARNING) << "Warning: Deleting "
                    << "BluetoothLowEnergyWeaveClientConnection object with an "
                    << "active connection to " << GetDeviceInfoLogString()
                    << ". This may result in disconnection errors; trying to "
                    << "send uWeave \"connection close\" packet before "
                    << "deleting.";
    ClearQueueAndSendConnectionClose();
  }

  DestroyConnection(
      BleWeaveConnectionResult::BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

void BluetoothLowEnergyWeaveClientConnection::Connect() {
  DCHECK(sub_status() == SubStatus::DISCONNECTED);

  if (should_set_low_connection_latency_)
    SetConnectionLatency();
  else
    CreateGattConnection();
}

void BluetoothLowEnergyWeaveClientConnection::SetConnectionLatency() {
  DCHECK(sub_status() == SubStatus::DISCONNECTED);
  SetSubStatus(SubStatus::WAITING_CONNECTION_LATENCY);

  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device) {
    PA_LOG(WARNING) << "Device not found; cannot set connection latency for "
                    << GetDeviceInfoLogString() << ".";
    DestroyConnection(
        BleWeaveConnectionResult::
            BLE_WEAVE_CONNECTION_RESULT_ERROR_BLUETOOTH_DEVICE_NOT_AVAILABLE);
    return;
  }

  bluetooth_device->SetConnectionLatency(
      device::BluetoothDevice::ConnectionLatency::CONNECTION_LATENCY_LOW,
      base::BindRepeating(&BluetoothLowEnergyWeaveClientConnection::
                              OnSetConnectionLatencySuccess,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&BluetoothLowEnergyWeaveClientConnection::
                              OnSetConnectionLatencyErrorOrTimeout,
                          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::CreateGattConnection() {
  DCHECK(sub_status() == SubStatus::DISCONNECTED ||
         sub_status() == SubStatus::WAITING_CONNECTION_LATENCY);
  SetSubStatus(SubStatus::WAITING_GATT_CONNECTION);

  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device) {
    PA_LOG(WARNING) << "Device not found; cannot create GATT connection to "
                    << GetDeviceInfoLogString() << ".";
    DestroyConnection(
        BleWeaveConnectionResult::
            BLE_WEAVE_CONNECTION_RESULT_ERROR_BLUETOOTH_DEVICE_NOT_AVAILABLE);
    return;
  }

  PA_LOG(INFO) << "Creating GATT connection with " << GetDeviceInfoLogString()
               << ".";
  bluetooth_device->CreateGattConnection(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnGattConnectionCreated,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnCreateGattConnectionError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::Disconnect() {
  if (IsConnected()) {
    // If a disconnection is already in progress, there is nothing to do.
    if (has_triggered_disconnection_)
      return;

    has_triggered_disconnection_ = true;
    PA_LOG(INFO) << "Disconnection requested; sending \"connection close\" "
                 << "uWeave packet to " << GetDeviceInfoLogString() << ".";

    // Send a "connection close" uWeave packet. After the send has completed,
    // the connection will disconnect automatically.
    ClearQueueAndSendConnectionClose();
    return;
  }

  DestroyConnection(
      BleWeaveConnectionResult::BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
}

void BluetoothLowEnergyWeaveClientConnection::DestroyConnection(
    BleWeaveConnectionResult result) {
  if (!has_recorded_connection_result_) {
    has_recorded_connection_result_ = true;
    RecordBleWeaveConnectionResult(result);
  }

  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  notify_session_.reset();
  characteristic_finder_.reset();

  if (gatt_connection_) {
    PA_LOG(INFO) << "Disconnecting from " << GetDeviceInfoLogString();
    gatt_connection_.reset();
  }

  SetSubStatus(SubStatus::DISCONNECTED);
}

void BluetoothLowEnergyWeaveClientConnection::SetSubStatus(
    SubStatus new_sub_status) {
  sub_status_ = new_sub_status;
  timer_->Stop();

  base::TimeDelta timeout_for_sub_status = GetTimeoutForSubStatus(sub_status_);
  if (!timeout_for_sub_status.is_max()) {
    timer_->Start(
        FROM_HERE, timeout_for_sub_status,
        base::BindOnce(
            &BluetoothLowEnergyWeaveClientConnection::OnTimeoutForSubStatus,
            weak_ptr_factory_.GetWeakPtr(), sub_status_));
  }

  // Sets the status of base class Connection.
  switch (new_sub_status) {
    case SubStatus::CONNECTED_AND_IDLE:
    case SubStatus::CONNECTED_AND_SENDING_MESSAGE:
      SetStatus(Status::CONNECTED);
      break;
    case SubStatus::DISCONNECTED:
      SetStatus(Status::DISCONNECTED);
      break;
    default:
      SetStatus(Status::IN_PROGRESS);
  }
}

void BluetoothLowEnergyWeaveClientConnection::OnTimeoutForSubStatus(
    SubStatus timed_out_sub_status) {
  // Ensure that |timed_out_sub_status| is still the active status.
  DCHECK(timed_out_sub_status == sub_status());

  if (timed_out_sub_status == SubStatus::WAITING_CONNECTION_LATENCY) {
    OnSetConnectionLatencyErrorOrTimeout();
    return;
  }

  PA_LOG(ERROR) << "Timed out waiting during SubStatus "
                << SubStatusToString(timed_out_sub_status) << ". "
                << "Destroying connection.";

  BleWeaveConnectionResult result =
      BleWeaveConnectionResult::BLE_WEAVE_CONNECTION_RESULT_MAX;
  switch (timed_out_sub_status) {
    case SubStatus::WAITING_GATT_CONNECTION:
      result = BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_CREATING_GATT_CONNECTION;
      break;
    case SubStatus::WAITING_CHARACTERISTICS:
      result = BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_FINDING_GATT_CHARACTERISTICS;
      break;
    case SubStatus::WAITING_NOTIFY_SESSION:
      result = BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_STARTING_NOTIFY_SESSION;
      break;
    case SubStatus::WAITING_CONNECTION_RESPONSE:
      result = BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_WAITING_FOR_CONNECTION_RESPONSE;
      break;
    case SubStatus::CONNECTED_AND_SENDING_MESSAGE:
      result = BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_TIMEOUT_WAITING_FOR_MESSAGE_TO_SEND;
      break;
    default:
      NOTREACHED();
  }

  DestroyConnection(result);
}

void BluetoothLowEnergyWeaveClientConnection::SetupTestDoubles(
    scoped_refptr<base::TaskRunner> test_task_runner,
    std::unique_ptr<base::OneShotTimer> test_timer,
    std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> test_generator,
    std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> test_receiver) {
  task_runner_ = test_task_runner;
  timer_ = std::move(test_timer);
  packet_generator_ = std::move(test_generator);
  packet_receiver_ = std::move(test_receiver);
}

void BluetoothLowEnergyWeaveClientConnection::SendMessageImpl(
    std::unique_ptr<WireMessage> message) {
  DCHECK(IsConnected());

  // Split |message| up into multiple packets which can be sent as one uWeave
  // message.
  std::vector<Packet> weave_packets =
      packet_generator_->EncodeDataMessage(message->Serialize());

  // For each packet, create a WriteRequest and add it to the queue.
  for (uint32_t i = 0; i < weave_packets.size(); ++i) {
    WriteRequestType request_type = (i != weave_packets.size() - 1)
                                        ? WriteRequestType::REGULAR
                                        : WriteRequestType::MESSAGE_COMPLETE;
    queued_write_requests_.emplace(std::make_unique<WriteRequest>(
        weave_packets[i], request_type, message.get()));
  }

  // Add |message| to the queue of WireMessages.
  queued_wire_messages_.emplace(std::move(message));

  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  // Ignore updates about other devices.
  if (device->GetAddress() != GetDeviceAddress())
    return;

  if (sub_status() == SubStatus::DISCONNECTED ||
      sub_status() == SubStatus::WAITING_CONNECTION_LATENCY ||
      sub_status() == SubStatus::WAITING_GATT_CONNECTION) {
    // Ignore status change events if a connection has not yet occurred.
    return;
  }

  // If a connection has already occurred and |device| is still connected, there
  // is nothing to do.
  if (is_now_connected)
    return;

  PA_LOG(WARNING) << "GATT connection to " << GetDeviceInfoLogString()
                  << " has been dropped.";
  DestroyConnection(BleWeaveConnectionResult::
                        BLE_WEAVE_CONNECTION_RESULT_ERROR_CONNECTION_DROPPED);
}

void BluetoothLowEnergyWeaveClientConnection::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const Packet& value) {
  DCHECK_EQ(adapter, adapter_.get());

  // Ignore characteristics which do not apply to this connection.
  if (characteristic->GetIdentifier() != rx_characteristic_.id)
    return;

  if (sub_status() != SubStatus::WAITING_CONNECTION_RESPONSE &&
      !IsConnected()) {
    PA_LOG(WARNING) << "Received message from " << GetDeviceInfoLogString()
                    << ", but was not expecting one. sub_status() = "
                    << sub_status();
    return;
  }

  switch (packet_receiver_->ReceivePacket(value)) {
    case ReceiverState::DATA_READY:
      OnBytesReceived(packet_receiver_->GetDataMessage());
      break;
    case ReceiverState::CONNECTION_CLOSED:
      PA_LOG(INFO) << "Received \"connection close\" uWeave packet from "
                   << GetDeviceInfoLogString()
                   << ". Reason: " << GetReasonForClose() << ".";
      DestroyConnection(BleWeaveConnectionResult::
                            BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
      return;
    case ReceiverState::ERROR_DETECTED:
      PA_LOG(ERROR) << "Received invalid packet from "
                    << GetDeviceInfoLogString() << ". ";
      ClearQueueAndSendConnectionClose();
      break;
    case ReceiverState::WAITING:
      CompleteConnection();
      break;
    case ReceiverState::RECEIVING_DATA:
      // Continue to wait for more packets to arrive; once the rest of the
      // packets for this message are received, |packet_receiver_| will
      // transition to the DATA_READY state.
      break;
    default:
      NOTREACHED();
  }
}

void BluetoothLowEnergyWeaveClientConnection::CompleteConnection() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE);

  uint16_t max_packet_size = packet_receiver_->GetMaxPacketSize();
  PA_LOG(INFO) << "Received uWeave \"connection response\" packet; connection "
               << "is now fully initialized for " << GetDeviceInfoLogString()
               << ". Max packet size: " << max_packet_size;

  // Now that the "connection close" uWeave packet has been received,
  // |packet_receiver_| should have received a max packet size from the GATT
  // server.
  packet_generator_->SetMaxPacketSize(max_packet_size);

  SetSubStatus(SubStatus::CONNECTED_AND_IDLE);
}

void BluetoothLowEnergyWeaveClientConnection::OnSetConnectionLatencySuccess() {
  // TODO(crbug.com/929518): Record how long it took to set connection latency.

  // It's possible that we timed out when attempting to set the connection
  // latency before this callback was called, resulting in this class moving
  // forward with a GATT connection (i.e., in any state other than
  // |SubStatus::WAITING_CONNECTION_LATENCY|). That could mean, at this point in
  // time, that we're in the middle of connecting, already connected, or any
  // state in between. That's fine; simply early return in order to prevent
  // trying to create yet another GATT connection (which will fail).
  if (sub_status() != SubStatus::WAITING_CONNECTION_LATENCY) {
    PA_LOG(WARNING) << "Setting connection latency succeeded but GATT "
                    << "connection to " << GetDeviceInfoLogString()
                    << " is already in progress or complete.";
    return;
  }

  CreateGattConnection();
}

void BluetoothLowEnergyWeaveClientConnection::
    OnSetConnectionLatencyErrorOrTimeout() {
  // TODO(crbug.com/929518): Record when setting connection latency fails or
  // times out.

  DCHECK(sub_status_ == SubStatus::WAITING_CONNECTION_LATENCY);
  PA_LOG(WARNING)
      << "Error or timeout setting connection latency for connection to "
      << GetDeviceInfoLogString() << ".";

  // Even if setting the connection latency fails, continue with the
  // connection. This is unfortunate but should not be considered a fatal error.
  CreateGattConnection();
}

void BluetoothLowEnergyWeaveClientConnection::OnCreateGattConnectionError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK(sub_status_ == SubStatus::WAITING_GATT_CONNECTION);
  RecordGattConnectionResult(
      BluetoothDeviceConnectErrorCodeToGattConnectionResult(error_code));
  PA_LOG(WARNING) << "Error creating GATT connection to "
                  << GetDeviceInfoLogString() << ". Error code: " << error_code;
  DestroyConnection(
      BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_CREATING_GATT_CONNECTION);
}

void BluetoothLowEnergyWeaveClientConnection::OnGattConnectionCreated(
    std::unique_ptr<device::BluetoothGattConnection> gatt_connection) {
  DCHECK(sub_status() == SubStatus::WAITING_GATT_CONNECTION);
  RecordGattConnectionResult(
      GattConnectionResult::GATT_CONNECTION_RESULT_SUCCESS);

  gatt_connection_ = std::move(gatt_connection);
  SetSubStatus(SubStatus::WAITING_CHARACTERISTICS);

  PA_LOG(INFO) << "Finding GATT characteristics for "
               << GetDeviceInfoLogString() << ".";
  characteristic_finder_.reset(CreateCharacteristicsFinder(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFound,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnCharacteristicsFinderError,
                 weak_ptr_factory_.GetWeakPtr())));
}

BluetoothLowEnergyCharacteristicsFinder*
BluetoothLowEnergyWeaveClientConnection::CreateCharacteristicsFinder(
    const BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
        success_callback,
    const BluetoothLowEnergyCharacteristicsFinder::ErrorCallback&
        error_callback) {
  return new BluetoothLowEnergyCharacteristicsFinder(
      adapter_, GetBluetoothDevice(), remote_service_, tx_characteristic_,
      rx_characteristic_, success_callback, error_callback, remote_device(),
      std::make_unique<BackgroundEidGenerator>());
}

void BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFound(
    const RemoteAttribute& service,
    const RemoteAttribute& tx_characteristic,
    const RemoteAttribute& rx_characteristic) {
  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);

  remote_service_ = service;
  tx_characteristic_ = tx_characteristic;
  rx_characteristic_ = rx_characteristic;
  characteristic_finder_.reset();

  SetSubStatus(SubStatus::CHARACTERISTICS_FOUND);

  StartNotifySession();
}

void BluetoothLowEnergyWeaveClientConnection::OnCharacteristicsFinderError() {
  DCHECK(sub_status() == SubStatus::WAITING_CHARACTERISTICS);

  PA_LOG(ERROR) << "Could not find GATT characteristics for "
                << GetDeviceInfoLogString();

  characteristic_finder_.reset();

  DestroyConnection(
      BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_FINDING_GATT_CHARACTERISTICS);
}

void BluetoothLowEnergyWeaveClientConnection::StartNotifySession() {
  DCHECK(sub_status() == SubStatus::CHARACTERISTICS_FOUND);

  device::BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(rx_characteristic_.id);
  if (!characteristic) {
    PA_LOG(ERROR) << "Characteristic no longer available after it was found. "
                  << "Cannot start notification session for "
                  << GetDeviceInfoLogString() << ".";
    DestroyConnection(
        BleWeaveConnectionResult::
            BLE_WEAVE_CONNECTION_RESULT_ERROR_GATT_CHARACTERISTIC_NOT_AVAILABLE);
    return;
  }

  // Workaround for crbug.com/507325. If |characteristic| is already notifying,
  // characteristic->StartNotifySession() fails with GATT_ERROR_FAILED.
  if (characteristic->IsNotifying()) {
    SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
    SendConnectionRequest();
    return;
  }

  SetSubStatus(SubStatus::WAITING_NOTIFY_SESSION);
  PA_LOG(INFO) << "Starting notification session for "
               << GetDeviceInfoLogString() << ".";
  characteristic->StartNotifySession(
      base::Bind(
          &BluetoothLowEnergyWeaveClientConnection::OnNotifySessionStarted,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::OnNotifySessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::OnNotifySessionStarted(
    std::unique_ptr<device::BluetoothGattNotifySession> notify_session) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  RecordGattNotifySessionResult(
      GattServiceOperationResult::GATT_SERVICE_OPERATION_RESULT_SUCCESS);
  notify_session_ = std::move(notify_session);
  SetSubStatus(SubStatus::NOTIFY_SESSION_READY);
  SendConnectionRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnNotifySessionError(
    device::BluetoothRemoteGattService::GattErrorCode error) {
  DCHECK(sub_status() == SubStatus::WAITING_NOTIFY_SESSION);
  RecordGattNotifySessionResult(
      BluetoothRemoteDeviceGattServiceGattErrorCodeToGattServiceOperationResult(
          error));
  PA_LOG(ERROR) << "Cannot start notification session for "
                << GetDeviceInfoLogString() << ". Error: " << error << ".";
  DestroyConnection(
      BleWeaveConnectionResult::
          BLE_WEAVE_CONNECTION_RESULT_ERROR_STARTING_NOTIFY_SESSION);
}

void BluetoothLowEnergyWeaveClientConnection::SendConnectionRequest() {
  DCHECK(sub_status() == SubStatus::NOTIFY_SESSION_READY);
  SetSubStatus(SubStatus::WAITING_CONNECTION_RESPONSE);

  PA_LOG(INFO) << "Sending \"connection request\" uWeave packet to "
               << GetDeviceInfoLogString();

  queued_write_requests_.emplace(std::make_unique<WriteRequest>(
      packet_generator_->CreateConnectionRequest(),
      WriteRequestType::CONNECTION_REQUEST));
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::ProcessNextWriteRequest() {
  // If there is already an in-progress write or there are no pending
  // WriteRequests, there is nothing to do.
  if (pending_write_request_ || queued_write_requests_.empty())
    return;

  pending_write_request_ = std::move(queued_write_requests_.front());
  queued_write_requests_.pop();

  PA_LOG(INFO) << "Writing " << pending_write_request_->value.size() << " "
               << "bytes to " << GetDeviceInfoLogString() << ".";
  SendPendingWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::SendPendingWriteRequest() {
  DCHECK(pending_write_request_);

  device::BluetoothRemoteGattCharacteristic* characteristic =
      GetGattCharacteristic(tx_characteristic_.id);
  if (!characteristic) {
    PA_LOG(ERROR) << "Characteristic no longer available after it was found. "
                  << "Cannot process write request for "
                  << GetDeviceInfoLogString() << ".";
    DestroyConnection(
        BleWeaveConnectionResult::
            BLE_WEAVE_CONNECTION_RESULT_ERROR_GATT_CHARACTERISTIC_NOT_AVAILABLE);
    return;
  }

  // If the current status is CONNECTED_AND_IDLE, transition to
  // CONNECTED_AND_SENDING_MESSAGE. This function also runs when the status is
  // WAITING_CONNECTION_RESPONSE; in that case, the status should remain
  // unchanged until the connection response has been received.
  if (sub_status() == SubStatus::CONNECTED_AND_IDLE)
    SetSubStatus(SubStatus::CONNECTED_AND_SENDING_MESSAGE);

  characteristic->WriteRemoteCharacteristic(
      pending_write_request_->value,
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnRemoteCharacteristicWritten,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::
                     OnWriteRemoteCharacteristicError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyWeaveClientConnection::OnRemoteCharacteristicWritten() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE ||
         sub_status() == SubStatus::CONNECTED_AND_SENDING_MESSAGE);
  if (sub_status() == SubStatus::CONNECTED_AND_SENDING_MESSAGE)
    SetSubStatus(SubStatus::CONNECTED_AND_IDLE);

  RecordGattWriteCharacteristicResult(
      GattServiceOperationResult::GATT_SERVICE_OPERATION_RESULT_SUCCESS);

  if (!pending_write_request_) {
    PA_LOG(ERROR) << "OnRemoteCharacteristicWritten() called, but no pending "
                  << "WriteRequest. Stopping connection to "
                  << GetDeviceInfoLogString();
    DestroyConnection(
        BleWeaveConnectionResult::
            BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITE_QUEUE_OUT_OF_SYNC);
    return;
  }

  if (pending_write_request_->request_type ==
      WriteRequestType::CONNECTION_CLOSE) {
    // Once a "connection close" uWeave packet has been sent, the connection
    // is ready to be disconnected.
    PA_LOG(INFO) << "uWeave \"connection close\" packet sent to "
                 << GetDeviceInfoLogString() << ". Destroying connection.";
    DestroyConnection(
        BleWeaveConnectionResult::BLE_WEAVE_CONNECTION_RESULT_CLOSED_NORMALLY);
    return;
  }

  if (pending_write_request_->request_type ==
      WriteRequestType::MESSAGE_COMPLETE) {
    if (queued_wire_messages_.empty()) {
      PA_LOG(ERROR) << "Sent a WriteRequest with type == MESSAGE_COMPLETE, but "
                    << "there were no queued WireMessages. Cannot process "
                    << "completed write to " << GetDeviceInfoLogString();
      DestroyConnection(
          BleWeaveConnectionResult::
              BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITING_GATT_CHARACTERISTIC);
      return;
    }

    std::unique_ptr<WireMessage> sent_message =
        std::move(queued_wire_messages_.front());
    queued_wire_messages_.pop();
    DCHECK_EQ(sent_message.get(),
              pending_write_request_->associated_wire_message);

    // Notify observers of the message being sent via a task on the run loop to
    // ensure that if an observer deletes this object in response to receiving
    // the OnSendCompleted() callback, a null pointer is not deferenced.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothLowEnergyWeaveClientConnection::OnDidSendMessage,
            weak_ptr_factory_.GetWeakPtr(), *sent_message, true /* success */));
  }

  pending_write_request_.reset();
  ProcessNextWriteRequest();
}

void BluetoothLowEnergyWeaveClientConnection::OnWriteRemoteCharacteristicError(
    device::BluetoothRemoteGattService::GattErrorCode error) {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE ||
         sub_status() == SubStatus::CONNECTED_AND_SENDING_MESSAGE);
  if (sub_status() == SubStatus::CONNECTED_AND_SENDING_MESSAGE)
    SetSubStatus(SubStatus::CONNECTED_AND_IDLE);

  RecordGattWriteCharacteristicResult(
      BluetoothRemoteDeviceGattServiceGattErrorCodeToGattServiceOperationResult(
          error));

  if (!pending_write_request_) {
    PA_LOG(ERROR) << "OnWriteRemoteCharacteristicError() called, but no "
                  << "pending WriteRequest. Stopping connection to "
                  << GetDeviceInfoLogString();
    DestroyConnection(
        BleWeaveConnectionResult::
            BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITE_QUEUE_OUT_OF_SYNC);
    return;
  }

  ++pending_write_request_->number_of_failed_attempts;
  if (pending_write_request_->number_of_failed_attempts <
      kMaxNumberOfRetryAttempts + 1) {
    PA_LOG(WARNING) << "Error sending WriteRequest to "
                    << GetDeviceInfoLogString() << "; failure number "
                    << pending_write_request_->number_of_failed_attempts
                    << ". Retrying.";
    SendPendingWriteRequest();
    return;
  }

  if (pending_write_request_->request_type == WriteRequestType::REGULAR ||
      pending_write_request_->request_type ==
          WriteRequestType::MESSAGE_COMPLETE) {
    std::unique_ptr<WireMessage> failed_message =
        std::move(queued_wire_messages_.front());
    queued_wire_messages_.pop();
    DCHECK_EQ(failed_message.get(),
              pending_write_request_->associated_wire_message);

    OnDidSendMessage(*failed_message, false /* success */);
  }

  // Since the try limit has been hit, this is a fatal error. Destroy the
  // connection, but post it as a new task to ensure that observers have a
  // chance to process the OnSendCompleted() call.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BluetoothLowEnergyWeaveClientConnection::DestroyConnection,
          weak_ptr_factory_.GetWeakPtr(),
          BleWeaveConnectionResult::
              BLE_WEAVE_CONNECTION_RESULT_ERROR_WRITING_GATT_CHARACTERISTIC));
}

void BluetoothLowEnergyWeaveClientConnection::OnDidSendMessage(
    const WireMessage& message,
    bool success) {
  Connection::OnDidSendMessage(message, success);
}

void BluetoothLowEnergyWeaveClientConnection::
    ClearQueueAndSendConnectionClose() {
  DCHECK(sub_status() == SubStatus::WAITING_CONNECTION_RESPONSE ||
         IsConnected());

  // The connection is now in an invalid state. Clear queued writes.
  while (!queued_write_requests_.empty())
    queued_write_requests_.pop();

  // Now, queue up a "connection close" uWeave packet. If there was a pending
  // write, we must wait for it to complete before the "connection close" can
  // be sent.
  queued_write_requests_.emplace(
      std::make_unique<WriteRequest>(packet_generator_->CreateConnectionClose(
                                         packet_receiver_->GetReasonToClose()),
                                     WriteRequestType::CONNECTION_CLOSE));

  if (pending_write_request_) {
    PA_LOG(WARNING) << "Waiting for current write to complete, then will send "
                    << "a \"connection close\" uWeave packet to "
                    << GetDeviceInfoLogString() << ".";
  } else {
    PA_LOG(INFO) << "Sending a \"connection close\" uWeave packet to "
                 << GetDeviceInfoLogString() << ".";
  }

  ProcessNextWriteRequest();
}

std::string BluetoothLowEnergyWeaveClientConnection::GetDeviceAddress() {
  // When the remote device is connected, rely on the address given by
  // |gatt_connection_|. Unpaired BLE device addresses are ephemeral and are
  // expected to change periodically.
  return gatt_connection_ ? gatt_connection_->GetDeviceAddress()
                          : initial_device_address_;
}

void BluetoothLowEnergyWeaveClientConnection::GetConnectionRssi(
    base::OnceCallback<void(base::Optional<int32_t>)> callback) {
  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device || !bluetooth_device->IsConnected()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // device::BluetoothDevice has not converted to using a base::OnceCallback
  // instead of a base::Callback, so use a wrapper for now.
  auto callback_holder = base::AdaptCallbackForRepeating(std::move(callback));
  bluetooth_device->GetConnectionInfo(
      base::Bind(&BluetoothLowEnergyWeaveClientConnection::OnConnectionInfo,
                 weak_ptr_factory_.GetWeakPtr(), callback_holder));
}

void BluetoothLowEnergyWeaveClientConnection::OnConnectionInfo(
    base::RepeatingCallback<void(base::Optional<int32_t>)> rssi_callback,
    const device::BluetoothDevice::ConnectionInfo& connection_info) {
  if (connection_info.rssi == device::BluetoothDevice::kUnknownPower) {
    std::move(rssi_callback).Run(base::nullopt);
    return;
  }

  std::move(rssi_callback).Run(connection_info.rssi);
}

device::BluetoothRemoteGattService*
BluetoothLowEnergyWeaveClientConnection::GetRemoteService() {
  device::BluetoothDevice* bluetooth_device = GetBluetoothDevice();
  if (!bluetooth_device) {
    PA_LOG(WARNING) << "Cannot find Bluetooth device for "
                    << GetDeviceInfoLogString();
    return nullptr;
  }

  if (remote_service_.id.empty()) {
    for (auto* service : bluetooth_device->GetGattServices()) {
      if (service->GetUUID() == remote_service_.uuid) {
        remote_service_.id = service->GetIdentifier();
        break;
      }
    }
  }

  return bluetooth_device->GetGattService(remote_service_.id);
}

device::BluetoothRemoteGattCharacteristic*
BluetoothLowEnergyWeaveClientConnection::GetGattCharacteristic(
    const std::string& gatt_characteristic) {
  device::BluetoothRemoteGattService* remote_service = GetRemoteService();
  if (!remote_service) {
    PA_LOG(WARNING) << "Cannot find GATT service for "
                    << GetDeviceInfoLogString();
    return nullptr;
  }
  return remote_service->GetCharacteristic(gatt_characteristic);
}

device::BluetoothDevice*
BluetoothLowEnergyWeaveClientConnection::GetBluetoothDevice() {
  return adapter_ ? adapter_->GetDevice(GetDeviceAddress()) : nullptr;
}

std::string BluetoothLowEnergyWeaveClientConnection::GetReasonForClose() {
  switch (packet_receiver_->GetReasonForClose()) {
    case ReasonForClose::CLOSE_WITHOUT_ERROR:
      return "CLOSE_WITHOUT_ERROR";
    case ReasonForClose::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    case ReasonForClose::NO_COMMON_VERSION_SUPPORTED:
      return "NO_COMMON_VERSION_SUPPORTED";
    case ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE:
      return "RECEIVED_PACKET_OUT_OF_SEQUENCE";
    case ReasonForClose::APPLICATION_ERROR:
      return "APPLICATION_ERROR";
    default:
      NOTREACHED();
      return "";
  }
}

void BluetoothLowEnergyWeaveClientConnection::RecordBleWeaveConnectionResult(
    BleWeaveConnectionResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "ProximityAuth.BleWeaveConnectionResult", result,
      BleWeaveConnectionResult::BLE_WEAVE_CONNECTION_RESULT_MAX);
}

void BluetoothLowEnergyWeaveClientConnection::RecordGattConnectionResult(
    GattConnectionResult result) {
  UMA_HISTOGRAM_ENUMERATION("ProximityAuth.BluetoothGattConnectionResult",
                            result,
                            GattConnectionResult::GATT_CONNECTION_RESULT_MAX);
}

BluetoothLowEnergyWeaveClientConnection::GattConnectionResult
BluetoothLowEnergyWeaveClientConnection::
    BluetoothDeviceConnectErrorCodeToGattConnectionResult(
        device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_AUTH_CANCELED;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_AUTH_FAILED;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_AUTH_REJECTED;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_AUTH_TIMEOUT;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_FAILED;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_INPROGRESS;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN:
      return GattConnectionResult::GATT_CONNECTION_RESULT_ERROR_UNKNOWN;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE:
      return GattConnectionResult::
          GATT_CONNECTION_RESULT_ERROR_UNSUPPORTED_DEVICE;
    default:
      return GattConnectionResult::GATT_CONNECTION_RESULT_UNKNOWN;
  }
}

void BluetoothLowEnergyWeaveClientConnection::RecordGattNotifySessionResult(
    GattServiceOperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "ProximityAuth.BluetoothGattNotifySessionResult", result,
      GattServiceOperationResult::GATT_SERVICE_OPERATION_RESULT_MAX);
}

void BluetoothLowEnergyWeaveClientConnection::
    RecordGattWriteCharacteristicResult(GattServiceOperationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "ProximityAuth.BluetoothGattWriteCharacteristicResult", result,
      GattServiceOperationResult::GATT_SERVICE_OPERATION_RESULT_MAX);
}

BluetoothLowEnergyWeaveClientConnection::GattServiceOperationResult
BluetoothLowEnergyWeaveClientConnection::
    BluetoothRemoteDeviceGattServiceGattErrorCodeToGattServiceOperationResult(
        device::BluetoothRemoteGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothRemoteGattService::GattErrorCode::GATT_ERROR_UNKNOWN:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_UNKNOWN;
    case device::BluetoothRemoteGattService::GattErrorCode::GATT_ERROR_FAILED:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_FAILED;
    case device::BluetoothRemoteGattService::GattErrorCode::
        GATT_ERROR_IN_PROGRESS:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_IN_PROGRESS;
    case device::BluetoothRemoteGattService::GattErrorCode::
        GATT_ERROR_INVALID_LENGTH:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_INVALID_LENGTH;
    case device::BluetoothRemoteGattService::GattErrorCode::
        GATT_ERROR_NOT_PERMITTED:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_NOT_PERMITTED;
    case device::BluetoothRemoteGattService::GattErrorCode::
        GATT_ERROR_NOT_AUTHORIZED:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_NOT_AUTHORIZED;
    case device::BluetoothRemoteGattService::GattErrorCode::
        GATT_ERROR_NOT_PAIRED:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_NOT_PAIRED;
    case device::BluetoothRemoteGattService::GattErrorCode::
        GATT_ERROR_NOT_SUPPORTED:
      return GattServiceOperationResult::
          GATT_SERVICE_OPERATION_RESULT_GATT_ERROR_NOT_SUPPORTED;
    default:
      return GattServiceOperationResult::GATT_SERVICE_OPERATION_RESULT_UNKNOWN;
  }
}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::WriteRequest(
    const Packet& val,
    WriteRequestType request_type,
    WireMessage* associated_wire_message)
    : value(val),
      request_type(request_type),
      associated_wire_message(associated_wire_message) {}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::WriteRequest(
    const Packet& val,
    WriteRequestType request_type)
    : WriteRequest(val, request_type, nullptr /* associated_wire_message */) {}

BluetoothLowEnergyWeaveClientConnection::WriteRequest::~WriteRequest() {}

}  // namespace weave

}  // namespace secure_channel

}  // namespace chromeos
