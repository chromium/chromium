// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_

#include "chromeos/ash/components/hid_detection/bluetooth_hid_detector.h"

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::hid_detection {

// Concrete BluetoothHidDetector implementation that uses CrosBluetoothConfig.
class BluetoothHidDetectorImpl
    : public BluetoothHidDetector,
      public bluetooth_config::mojom::SystemPropertiesObserver,
      public bluetooth_config::mojom::BluetoothDiscoveryDelegate,
      public bluetooth_config::mojom::DevicePairingDelegate,
      public bluetooth_config::mojom::KeyEnteredHandler {
 public:
  BluetoothHidDetectorImpl();
  ~BluetoothHidDetectorImpl() override;

  // BluetoothHidDetector:
  void SetInputDevicesStatus(InputDevicesStatus input_devices_status) override;
  const BluetoothHidDetectionStatus GetBluetoothHidDetectionStatus() override;

 private:
  friend class BluetoothHidDetectorImplTest;

  static constexpr base::TimeDelta kMaxPairingSessionDuration =
      base::Seconds(33);

  // States used for internal state machine.
  enum State {
    // HID detection is currently not active.
    kNotStarted,

    // HID detection has began activating.
    kStarting,

    // HID detection has began activating and is waiting for the Bluetooth
    // adapter to be enabled.
    kEnablingAdapter,

    // HID detection is fully active and is now searching for devices.
    kDetecting,

    // HID detection is paused due to the Bluetooth adapter becoming unenabled
    // for external reasons.
    kStoppedExternally,
  };

  // BluetoothHidDetector:
  void PerformStartBluetoothHidDetection(
      InputDevicesStatus input_devices_status) override;
  void PerformStopBluetoothHidDetection(bool is_using_bluetooth) override;

  // bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  // bluetooth_config::mojom::BluetoothDiscoveryDelegate
  void OnBluetoothDiscoveryStarted(
      mojo::PendingRemote<bluetooth_config::mojom::DevicePairingHandler>
          handler) override;
  void OnBluetoothDiscoveryStopped() override;
  void OnDiscoveredDevicesListChanged(
      std::vector<bluetooth_config::mojom::BluetoothDevicePropertiesPtr>
          discovered_devices) override;

  // bluetooth_config::mojom::DevicePairingDelegate
  void RequestPinCode(RequestPinCodeCallback callback) override;
  void RequestPasskey(RequestPasskeyCallback callback) override;
  void DisplayPinCode(
      const std::string& pin_code,
      mojo::PendingReceiver<bluetooth_config::mojom::KeyEnteredHandler> handler)
      override;
  void DisplayPasskey(
      const std::string& passkey,
      mojo::PendingReceiver<bluetooth_config::mojom::KeyEnteredHandler> handler)
      override;
  void ConfirmPasskey(const std::string& passkey,
                      ConfirmPasskeyCallback callback) override;
  void AuthorizePairing(AuthorizePairingCallback callback) override;

  // bluetooth_config::mojom::KeyEnteredHandler
  void HandleKeyEntered(uint8_t num_keys_entered) override;

  bool IsHidTypeMissing(BluetoothHidDetector::BluetoothHidType hid_type);
  bool ShouldAttemptToPairWithDevice(
      const bluetooth_config::mojom::BluetoothDevicePropertiesPtr& device);

  void ProcessQueue();
  void OnPairDevice(std::unique_ptr<base::ElapsedTimer> pairing_timer,
                    bluetooth_config::mojom::PairingResult pairing_result);
  void OnPairingTimeout();

  // Removes any state related to the current pairing device. This will cancel
  // pairing with the device if there is an ongoing pairing.
  void ClearCurrentPairingState();

  // Resets properties related to discovery, pairing handlers and queueing.
  void ResetDiscoveryState();

  // Informs the client "DisplayPasskey" or "DisplayPinCode" pairing
  // authorization is required.
  void RequirePairingCode(
      const std::string& code,
      mojo::PendingReceiver<bluetooth_config::mojom::KeyEnteredHandler>
          handler);

  // Map that contains the ids of the devices in |queue_|.
  base::flat_set<std::string> queued_device_ids_;

  // The queue of devices that will be attempted to be paired with.
  std::unique_ptr<
      base::queue<bluetooth_config::mojom::BluetoothDevicePropertiesPtr>>
      queue_ = std::make_unique<
          base::queue<bluetooth_config::mojom::BluetoothDevicePropertiesPtr>>();

  // The device currently being paired with.
  std::optional<bluetooth_config::mojom::BluetoothDevicePropertiesPtr>
      current_pairing_device_;

  // If defined, indicates that the current pairing requires an authorization
  // code that should be displayed to the user for them to enter into the HID.
  std::optional<BluetoothHidPairingState> current_pairing_state_;

  // A timer started when the current pairing begins. If the pairing session
  // finishes, the timer's callback is invalidated. If the timer exceeds
  // kMaxPairingDuration, the current pairing session will be canceled.
  base::OneShotTimer current_pairing_timer_;

  InputDevicesStatus input_devices_status_;
  State state_ = kNotStarted;

  // This is a counter used to emit a count of the number of pairing attempts
  // that occur while HID detection is active. The count is reset to zero each
  // time a HID detection session is started.
  size_t num_pairing_attempts_ = 0;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      cros_bluetooth_config_remote_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      system_properties_observer_receiver_{this};
  mojo::Receiver<bluetooth_config::mojom::BluetoothDiscoveryDelegate>
      bluetooth_discovery_delegate_receiver_{this};
  mojo::Remote<bluetooth_config::mojom::DevicePairingHandler>
      device_pairing_handler_remote_;
  mojo::Receiver<bluetooth_config::mojom::DevicePairingDelegate>
      device_pairing_delegate_receiver_{this};
  mojo::Receiver<bluetooth_config::mojom::KeyEnteredHandler>
      key_entered_handler_receiver_{this};

  base::WeakPtrFactory<BluetoothHidDetectorImpl> weak_ptr_factory_{this};
};

}  // namespace ash::hid_detection

#endif  // CHROMEOS_ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_
