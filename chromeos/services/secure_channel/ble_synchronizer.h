// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SYNCHRONIZER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SYNCHRONIZER_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/secure_channel/ble_synchronizer_base.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace chromeos {

namespace secure_channel {

// Concrete BleSynchronizerBase implementation.
class BleSynchronizer : public BleSynchronizerBase {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<BleSynchronizerBase> BuildInstance(
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

   private:
    static Factory* test_factory_;
  };

  ~BleSynchronizer() override;

 protected:
  explicit BleSynchronizer(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void ProcessQueue() override;

 private:
  friend class SecureChannelBleSynchronizerTest;

  // BLUETOOTH_ADVERTISEMENT_RESULT_UNKNOWN indicates that the Bluetooth
  // platform returned a code that is not recognized.
  enum BluetoothAdvertisementResult {
    SUCCESS = 0,
    ERROR_UNSUPPORTED_PLATFORM = 1,
    ERROR_ADVERTISEMENT_ALREADY_EXISTS = 2,
    ERROR_ADVERTISEMENT_DOES_NOT_EXIST = 3,
    ERROR_ADVERTISEMENT_INVALID_LENGTH = 4,
    ERROR_INVALID_ADVERTISEMENT_INTERVAL = 5,
    ERROR_RESET_ADVERTISING = 6,
    INVALID_ADVERTISEMENT_ERROR_CODE = 7,
    BLUETOOTH_ADVERTISEMENT_RESULT_UNKNOWN = 8,
    BLUETOOTH_ADVERTISEMENT_RESULT_MAX
  };

  void SetTestDoubles(std::unique_ptr<base::OneShotTimer> test_timer,
                      base::Clock* test_clock,
                      scoped_refptr<base::TaskRunner> test_task_runner);

  void OnAdvertisementRegistered(
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  void OnErrorRegisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);
  void OnAdvertisementUnregistered();
  void OnErrorUnregisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code);
  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnErrorStartingDiscoverySession();
  void OnDiscoverySessionStopped();
  void OnErrorStoppingDiscoverySession();

  void ScheduleCommandCompletion();
  void CompleteCurrentCommand();

  void RecordBluetoothAdvertisementRegistrationResult(
      BluetoothAdvertisementResult result);
  void RecordBluetoothAdvertisementUnregistrationResult(
      BluetoothAdvertisementResult result);
  BluetoothAdvertisementResult BluetoothAdvertisementErrorCodeToResult(
      device::BluetoothAdvertisement::ErrorCode error_code);
  void RecordDiscoverySessionStarted(bool success);
  void RecordDiscoverySessionStopped(bool success);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  std::unique_ptr<Command> current_command_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::Clock* clock_;
  scoped_refptr<base::TaskRunner> task_runner_;
  base::Time last_command_end_timestamp_;
  base::WeakPtrFactory<BleSynchronizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BleSynchronizer);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SYNCHRONIZER_H_
