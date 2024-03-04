// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_CARRIER_LOCK_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_CARRIER_LOCK_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/carrier_lock/common.h"
#include "chromeos/ash/components/network/network_3gpp_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "net/base/backoff_entry.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {
enum class CarrierLockResult;
class Network3gppHandler;
}  // namespace ash

namespace network {
class NetworkState;
class NetworkStateHandler;
class SharedURLLoaderFactory;
}  // namespace network

namespace session_manager {
class SessionManager;
}

namespace ash::carrier_lock {
class ProvisioningConfigFetcher;
class FcmTopicSubscriber;
class PsmClaimVerifier;

inline constexpr base::TimeDelta kRetryDelay = base::Seconds(15);
inline constexpr base::TimeDelta kConfigDelay = base::Minutes(1);
inline constexpr int kMaxRetries = 2;

enum class ModemLockStatus {
  kUnknown,
  kNotLocked,
  kCarrierLocked,
};

// State of Carrier Lock configuration process
enum class ConfigurationState {
  kNone,
  kInitialize,
  kPsmCheckClaim,
  kFcmGetToken,
  kRequestConfig,
  kSetupModem,
  kFcmCheckTopic,
  kFcmSubscribe,
  kDeviceLocked,
  kDeviceUnlocked,
  kFatalError
};

inline const char kLastConfigTimePref[] =
    "cellular.carrier_lock.last_configuration_time";
inline const char kDisableManagerPref[] = "cellular.carrier_lock.disable";
inline const char kLastImeiPref[] = "cellular.carrier_lock.last_imei";
inline const char kFcmTopicPref[] = "cellular.carrier_lock.fcm_topic";
inline const char kErrorCounterPref[] = "cellular.carrier_lock.error_counter";
inline const char kSignedConfigPref[] =
    "cellular.carrier_lock.signed_configuration";

// This is the main class that controls configuration process of cellular modem.
// It uses PsmClaimVerifier, ProvisioningConfigFetcher, Network3gppHandler and
// FcmTopicSubscriber to check whether the device should be Carrier Locked,
// download lock configuration, configure the modem and subscribe for
// notifications about configuration updates. (go/cros-carrier-lock-service)
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK) CarrierLockManager
    : public NetworkStateHandlerObserver,
      public session_manager::SessionManagerObserver {
 public:
  explicit CarrierLockManager(PrefService*);
  CarrierLockManager() = delete;
  CarrierLockManager(const CarrierLockManager&) = delete;
  CarrierLockManager& operator=(const CarrierLockManager&) = delete;
  ~CarrierLockManager() override;

  // Create a new instance of Carrier Lock Manager
  // For testing, use CreateForTesting()
  static std::unique_ptr<CarrierLockManager> Create(
      PrefService*,
      gcm::GCMDriver*,
      scoped_refptr<network::SharedURLLoaderFactory>);

  // Register local preferences to keep configuration state between reboots
  static void RegisterLocalPrefs(PrefRegistrySimple*);

  // Return current status of modem lock configuration
  static ModemLockStatus GetModemLockStatus();

  static std::unique_ptr<CarrierLockManager> CreateForTesting(
      PrefService*,
      Network3gppHandler*,
      std::unique_ptr<FcmTopicSubscriber>,
      std::unique_ptr<PsmClaimVerifier>,
      std::unique_ptr<ProvisioningConfigFetcher>);

 private:
  friend class CarrierLockManagerTest;

  // ash::NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState*) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  void Initialize();

  void RunStep(ConfigurationState);
  bool RetryStep();
  void CheckState();
  void LogError(Result result);

  void RequestConfig();
  void ConfigCallback(Result result);

  void CheckPsmClaim();
  void PsmCallback(Result result);

  void SetupModem();
  void SetupModemCallback(CarrierLockResult result);

  void GetFcmToken();
  void FcmTokenCallback(Result result);
  void CheckFcmTopic();
  void SubscribeFcmTopic();
  void FcmTopicCallback(Result result);
  void FcmNotification(bool is_from_topic);

  // Store a method pointer to quickly rebind last method in RetryStep().
  void (CarrierLockManager::*function_)();

  ConfigurationState configuration_state_;
  int remaining_retries_;

  raw_ptr<PrefService> local_state_;
  std::string serial_;
  std::string imei_;
  std::string fcm_token_;
  std::string manufacturer_;
  std::string model_;
  int error_counter_ = 0;
  bool is_first_setup_ = true;
  size_t trigger_first_run_ = 0;
  size_t trigger_network_ = 0;
  size_t trigger_retry_step_ = 0;
  size_t trigger_scheduler_ = 0;

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<Network3gppHandler> modem_handler_ = nullptr;
  raw_ptr<session_manager::SessionManager> session_manager_ = nullptr;

  std::unique_ptr<ProvisioningConfigFetcher> config_;
  std::unique_ptr<PsmClaimVerifier> psm_;
  std::unique_ptr<FcmTopicSubscriber> fcm_;

  net::BackoffEntry retry_backoff_;
  base::WeakPtrFactory<CarrierLockManager> weak_ptr_factory_{this};
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_CARRIER_LOCK_MANAGER_H_
