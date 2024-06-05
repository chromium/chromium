// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_pairing_handler_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/bluetooth_config/device_conversion_util.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::bluetooth_config {

namespace {

DevicePairingHandlerImpl::Factory* g_test_factory = nullptr;

device::BluetoothTransport GetBluetoothTransport(
    device::BluetoothTransport type) {
  switch (type) {
    case device::BLUETOOTH_TRANSPORT_CLASSIC:
      return device::BLUETOOTH_TRANSPORT_CLASSIC;
    case device::BLUETOOTH_TRANSPORT_LE:
      return device::BLUETOOTH_TRANSPORT_LE;
    case device::BLUETOOTH_TRANSPORT_DUAL:
      return device::BLUETOOTH_TRANSPORT_DUAL;
    default:
      return device::BLUETOOTH_TRANSPORT_INVALID;
  }
}

}  // namespace

// static
std::unique_ptr<DevicePairingHandler> DevicePairingHandlerImpl::Factory::Create(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    FastPairDelegate* fast_pair_delegate) {
  if (g_test_factory) {
    return g_test_factory->CreateInstance(
        std::move(pending_receiver), adapter_state_controller,
        bluetooth_adapter, fast_pair_delegate);
  }

  return base::WrapUnique(new DevicePairingHandlerImpl(
      std::move(pending_receiver), adapter_state_controller, bluetooth_adapter,
      fast_pair_delegate));
}

// static
void DevicePairingHandlerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

DevicePairingHandlerImpl::Factory::~Factory() = default;

// static
const base::TimeDelta DevicePairingHandlerImpl::kPairingFailureDelay =
    base::Milliseconds(500);

DevicePairingHandlerImpl::DevicePairingHandlerImpl(
    mojo::PendingReceiver<mojom::DevicePairingHandler> pending_receiver,
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    FastPairDelegate* fast_pair_delegate)
    : DevicePairingHandler(std::move(pending_receiver),
                           adapter_state_controller),
      bluetooth_adapter_(std::move(bluetooth_adapter)),
      fast_pair_delegate_(fast_pair_delegate) {}

DevicePairingHandlerImpl::~DevicePairingHandlerImpl() {
  // If we have a pairing attempt and this class is destroyed, cancel the
  // pairing.
  if (!current_pairing_device_id().empty()) {
    BLUETOOTH_LOG(EVENT)
        << "DevicePairingHandlerImpl is being destroyed while pairing with "
        << current_pairing_device_id() << ", canceling pairing";
    CancelPairing();
  }
}

void DevicePairingHandlerImpl::FetchDevice(const std::string& device_address,
                                           FetchDeviceCallback callback) {
  BLUETOOTH_LOG(EVENT) << "Fetching device with address: " << device_address;
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetAddress() != device_address)
      continue;

    std::move(callback).Run(
        GenerateBluetoothDeviceMojoProperties(device, fast_pair_delegate_));
    return;
  }
  BLUETOOTH_LOG(ERROR) << "Device with address: " << device_address
                       << " was not found";
  std::move(callback).Run(std::move(nullptr));
}

void DevicePairingHandlerImpl::PerformPairDevice(const std::string& device_id) {
  // Find the device and attempt to pair to it.
  device::BluetoothDevice* device = FindDevice(device_id);

  if (!device) {
    BLUETOOTH_LOG(ERROR) << "Pairing failed due to device not being "
                         << "found, identifier: " << device_id;
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kFailed);
    return;
  }

  device->Connect(
      /*delegate=*/this,
      base::BindOnce(&DevicePairingHandlerImpl::OnDeviceConnect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DevicePairingHandlerImpl::PerformFinishCurrentPairingRequest(
    std::optional<device::ConnectionFailureReason> failure_reason,
    base::TimeDelta duration) {
  // Reset state.
  is_canceling_pairing_ = false;
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id());

  device::BluetoothTransport transport =
      device ? device->GetType()
             : device::BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID;

  device::RecordPairingResult(failure_reason, GetBluetoothTransport(transport),
                              duration);
}

void DevicePairingHandlerImpl::CancelPairing() {
  is_canceling_pairing_ = true;
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id());
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "Could not cancel pairing for device to due device no longer being "
           "found, identifier: "
        << current_pairing_device_id();
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kNotFound);
    return;
  }

  // Cancelling the active pairing attempt will cause OnDeviceConnect() to fire
  // with an error code.
  device->CancelPairing();
}

void DevicePairingHandlerImpl::OnRequestPinCode(const std::string& pin_code) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id());
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnRequestPinCode failed due to device no longer being "
        << "found, identifier: " << current_pairing_device_id();
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kNotFound);
    return;
  }

  BLUETOOTH_LOG(USER) << "Received pin code " << pin_code << " for device "
                      << current_pairing_device_id();
  device->SetPinCode(pin_code);
}

void DevicePairingHandlerImpl::OnRequestPasskey(const std::string& passkey) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id());
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnRequestPasskey failed due to device no longer being "
        << "found, identifier: " << current_pairing_device_id();
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kNotFound);
    return;
  }

  uint32_t passkey_num;
  if (base::StringToUint(passkey, &passkey_num)) {
    BLUETOOTH_LOG(USER) << "Received passkey " << passkey_num << " for device "
                        << current_pairing_device_id();
    device->SetPasskey(passkey_num);
    return;
  }

  // If string to uint32_t conversion was unsuccessful, cancel the pairing.
  BLUETOOTH_LOG(ERROR) << "Converting " << passkey
                       << "to uint32_t failed, canceling pairing with "
                       << current_pairing_device_id();
  CancelPairing();
}

void DevicePairingHandlerImpl::OnConfirmPairing(bool confirmed) {
  BLUETOOTH_LOG(EVENT) << "OnConfirmPairing() called with confirmed: "
                       << confirmed;

  device::BluetoothDevice* device = FindDevice(current_pairing_device_id());
  if (!device) {
    BLUETOOTH_LOG(ERROR)
        << "OnConfirmPairing failed due to device no longer being "
        << "found, identifier: " << current_pairing_device_id();
    FinishCurrentPairingRequest(device::ConnectionFailureReason::kNotFound);
    return;
  }

  if (confirmed) {
    device->ConfirmPairing();
  } else {
    device->CancelPairing();
  }
}

void DevicePairingHandlerImpl::RequestPinCode(device::BluetoothDevice* device) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendRequestPinCode();
}

void DevicePairingHandlerImpl::RequestPasskey(device::BluetoothDevice* device) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendRequestPasskey();
}

void DevicePairingHandlerImpl::DisplayPinCode(device::BluetoothDevice* device,
                                              const std::string& pin_code) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendDisplayPinCode(pin_code);
}

void DevicePairingHandlerImpl::DisplayPasskey(device::BluetoothDevice* device,
                                              uint32_t passkey) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendDisplayPasskey(passkey);
}

void DevicePairingHandlerImpl::KeysEntered(device::BluetoothDevice* device,
                                           uint32_t entered) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendKeysEntered(entered);
}

void DevicePairingHandlerImpl::ConfirmPasskey(device::BluetoothDevice* device,
                                              uint32_t passkey) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendConfirmPasskey(passkey);
}

void DevicePairingHandlerImpl::AuthorizePairing(
    device::BluetoothDevice* device) {
  DCHECK(device->GetIdentifier() == current_pairing_device_id());
  SendAuthorizePairing();
}

void DevicePairingHandlerImpl::OnDeviceConnect(
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (!error_code.has_value()) {
    BLUETOOTH_LOG(EVENT) << "Device " << current_pairing_device_id()
                         << " successfully paired";
    FinishCurrentPairingRequest(std::nullopt);
    return;
  }

  // In some cases, device->Connect() will return a failure if the pairing
  // succeeded but the subsequent connection request returns with a failure.
  // Empirically, it's found that the device actually does connect, and
  // device->IsConnected() returns true. Wait |kPairingFailureDelay| to check if
  // the device is connected. Only do this if the failure is not due to a
  // pairing cancellation. If the pairing is canceled, we know for sure that the
  // device is not actually paired.
  // TODO(b/209531279): Remove this delay and |is_canceling_pairing_| when the
  // root cause of issue is fixed.
  if (!is_canceling_pairing_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DevicePairingHandlerImpl::HandlePairingFailed,
                       weak_ptr_factory_.GetWeakPtr(), error_code.value()),
        kPairingFailureDelay);
    return;
  }

  // Immediately handle pairing failures if pairing is being canceled, because
  // we know for sure that the device is not actually paired, and because if
  // the pairing is being canceled due to the handler being destroyed, if there
  // is a delay the failure will never be handled.
  HandlePairingFailed(error_code.value());
}

void DevicePairingHandlerImpl::HandlePairingFailed(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  device::BluetoothDevice* device = FindDevice(current_pairing_device_id());

  // In some cases, device->Connect() will return a failure if the pairing
  // succeeded but the subsequent connection request returns with a failure.
  // Empirically, it's found that the device actually does connect, and
  // device->IsConnected() returns true. Handle this case the
  // same as pairing succeeding if this wasn't a pairing cancellation.
  // TODO(b/209531279): Remove this when the root cause of issue is fixed.
  if (device && device->IsConnected() && !is_canceling_pairing_) {
    BLUETOOTH_LOG(EVENT)
        << device->GetAddress()
        << ": Pairing finished with an error code, but device "
        << "is connected. Handling like pairing succeeded. Error code: "
        << error_code;
    FinishCurrentPairingRequest(std::nullopt);
    return;
  }

  // We use |current_pairing_device_id()| since it conveys the same information
  // as the address and |device| could be |nullptr|.
  BLUETOOTH_LOG(ERROR) << current_pairing_device_id()
                       << ": Pairing failed with error code: " << error_code;

  FinishCurrentPairingRequest(GetConnectionFailureReason(error_code));
}

device::BluetoothDevice* DevicePairingHandlerImpl::FindDevice(
    const std::string& device_id) const {
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetIdentifier() != device_id)
      continue;
    return device;
  }
  return nullptr;
}

}  // namespace ash::bluetooth_config
