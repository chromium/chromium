// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_METRICS_H_

namespace ash::carrier_lock {

// These enums are reported in metrics. Entries must not be reordered or
// renumbered. Corresponding names in enums.xml: CellularCarrierLock*.
enum class ConfigurationResult {
  kModemLocked = 0,
  kModemNotLocked = 1,
  kModemUnlocked = 2,
  kModemRelocked = 3,
  kMaxValue = kModemRelocked
};

enum class FcmNotification {
  kUpdateProfile = 0,
  kUnlockDevice = 1,
  kUpdateProfileBeforeInit = 2,
  kUnlockDeviceBeforeInit = 3,
  kUpdateProfileWhileBusy = 4,
  kUnlockDeviceWhileBusy = 5,
  kMaxValue = kUnlockDeviceWhileBusy
};

enum class FcmResult {
  kRegistered = 0,
  kSubscribed = 1,
  kMaxValue = kSubscribed
};

enum class InitialState {
  kFirstConfiguration = 0,
  kObsoleteConfiguration = 1,
  kModemImeiChanged = 2,
  kAlreadyConfigured = 3,
  kEmptySignedConfig = 4,
  kMaxValue = kEmptySignedConfig
};

enum class ProvisioningResult {
  kConfigLocked = 0,
  kConfigUnlocked = 1,
  kConfigTempUnlocked = 2,
  kConfigInvalid = 3,
  kMaxValue = kConfigInvalid
};

enum class PsmResult {
  kDeviceUnlocked = 0,
  kDeviceLocked = 1,
  kMaxValue = kDeviceLocked
};

enum class LockState {
  kNotConfigured = 0,
  kCompatibleSim = 1,
  kIncompatibleSim = 2,
  kProperlyUnlocked = 3,
  kIncorrectlyLocked = 4,
  kMaxValue = kIncorrectlyLocked
};

const char kInitializationTriggerFirstRun[] =
    "Network.Cellular.CarrierLock.Initialization.FirstRun";
const char kInitializationTriggerNetwork[] =
    "Network.Cellular.CarrierLock.Initialization.Network";
const char kInitializationTriggerRetryStep[] =
    "Network.Cellular.CarrierLock.Initialization.RetryStep";
const char kInitializationTriggerScheduler[] =
    "Network.Cellular.CarrierLock.Initialization.Scheduler";

const char kConfigurationStateAfterInitialization[] =
    "Network.Cellular.CarrierLock.ConfigurationStateAfterInitialization";
const char kFcmNotificationType[] =
    "Network.Cellular.CarrierLock.FcmNotificationType";
const char kFcmCommunicationResult[] =
    "Network.Cellular.CarrierLock.FcmCommunicationResult";
const char kLockState[] = "Network.Cellular.CarrierLock.LockState";
const char kModemConfigurationResult[] =
    "Network.Cellular.CarrierLock.ModemConfigurationResult";
const char kNumConsecutiveConfigurationFailures[] =
    "Network.Cellular.CarrierLock.NumConsecutiveConfigurationFailures";
const char kNumConsecutiveFailuresBeforeLock[] =
    "Network.Cellular.CarrierLock.NumConsecutiveFailuresBeforeLock";
const char kNumConsecutiveFailuresBeforeUnlock[] =
    "Network.Cellular.CarrierLock.NumConsecutiveFailuresBeforeUnlock";
const char kProvisioningServerResponse[] =
    "Network.Cellular.CarrierLock.ProvisioningServerResponse";
const char kPsmClaimResponse[] =
    "Network.Cellular.CarrierLock.PsmClaimResponse";

const char kErrorPsmClaim[] = "Network.Cellular.CarrierLock.Error.PsmClaim";
const char kErrorProvisioning[] =
    "Network.Cellular.CarrierLock.Error.Provisioning";
const char kErrorFcmTopic[] = "Network.Cellular.CarrierLock.Error.FcmTopic";
const char kErrorModemSetup[] = "Network.Cellular.CarrierLock.Error.ModemSetup";
const char kErrorManagerInitialization[] =
    "Network.Cellular.CarrierLock.Error.ManagerInitialization";

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_METRICS_H_
