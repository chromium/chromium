// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/carrier_lock_manager.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/types/fixed_array.h"
#include "chromeos/ash/components/carrier_lock/carrier_lock.pb.h"
#include "chromeos/ash/components/carrier_lock/fcm_topic_subscriber_impl.h"
#include "chromeos/ash/components/carrier_lock/metrics.h"
#include "chromeos/ash/components/carrier_lock/provisioning_config_fetcher_impl.h"
#include "chromeos/ash/components/carrier_lock/psm_claim_verifier_impl.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_3gpp_handler.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::carrier_lock {

namespace {

CarrierLockManager* g_instance = nullptr;

// const values
const char kFcmAppId[] = "com.google.chromeos.carrier_lock";
const char kFcmSenderId[] = "727210445342";
constexpr base::TimeDelta kFcmTimeout = base::Days(30);
const char kCarrierLockType[] = "network-pin";
const char kFirmwareVariantPath[] =
    "/run/chromeos-config/v1/modem/firmware-variant";
const char kManufacturerNamePath[] =
    "/run/chromeos-config/v1/branding/oem-name";
const char kModelNamePath[] = "/run/chromeos-config/v1/name";
const char kMachineModelName[] = "model_name";
const char kMachineOemName[] = "oem_name";

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors before using exponential delay.
    60 * 1000,       // Initial delay of 60 seconds in ms.
    1.2,             // Factor by which the waiting time will be multiplied.
    0,               // Fuzzing percentage.
    30 * 60 * 1000,  // Maximum delay of 30 minutes in ms.
    -1,              // Never discard the entry.
    false,           // Always use initial delay.
};

constexpr std::string_view ConfigurationStateToStringView(
    ConfigurationState state) {
  switch (state) {
    case ConfigurationState::kNone:
      return "None";
    case ConfigurationState::kInitialize:
      return "Initialize";
    case ConfigurationState::kPsmCheckClaim:
      return "Check PSM claim";
    case ConfigurationState::kFcmGetToken:
      return "Get FCM token";
    case ConfigurationState::kRequestConfig:
      return "Request configuration";
    case ConfigurationState::kSetupModem:
      return "Setup modem locks";
    case ConfigurationState::kFcmCheckTopic:
      return "Check FCM topic";
    case ConfigurationState::kFcmSubscribe:
      return "Subscribe FCM topic";
    case ConfigurationState::kDeviceUnlocked:
      return "Device unlocked";
    case ConfigurationState::kDeviceLocked:
      return "Device locked";
    case ConfigurationState::kFatalError:
      return "Fatal error";
  }
}

constexpr std::string_view ResultToStringView(Result result) {
  switch (result) {
    case Result::kSuccess:
      return "Success";
    case Result::kInvalidSignature:
      return "Invalid signature in configuration";
    case Result::kInvalidImei:
      return "Invalid IMEI in configuration";
    case Result::kInvalidTimestamp:
      return "Invalid timestamp in configuration";
    case Result::kNetworkListTooLarge:
      return "Configuration network list too large";
    case Result::kAlgorithmNotSupported:
      return "Configuration algorithm not supported";
    case Result::kFeatureNotSupported:
      return "Configuration feature not supported";
    case Result::kDecodeOrParsingError:
      return "Configuration decode or parsing error";
    case Result::kHandlerNotInitialized:
      return "Modem handler not initialized";
    case Result::kOperationNotSupported:
      return "Modem operation not supported";
    case Result::kModemInternalError:
      return "Modem internal error";
    case Result::kInvalidNetworkHandler:
      return "Network handler not initialized";
    case Result::kInvalidModemHandler:
      return "Modem 3gpp handler not initialized";
    case Result::kInvalidAuxHandlers:
      return "Auxiliary classes not initialized";
    case Result::kModemNotFound:
      return "Modem not found or invalid";
    case Result::kSerialProviderFailed:
      return "Failed to get serial number";
    case Result::kHandlerBusy:
      return "Handler busy";
    case Result::kRequestFailed:
      return "Request failed";
    case Result::kInitializationFailed:
      return "Initialization failed";
    case Result::kConnectionError:
      return "Connection error";
    case Result::kInvalidInput:
      return "Invalid input parameters";
    case Result::kServerInternalError:
      return "Server internal error";
    case Result::kInvalidResponse:
      return "Invalid response from server";
    case Result::kCreatePsmClientFailed:
      return "Failed to create PSM client";
    case Result::kCreateOprfRequestFailed:
      return "Failed to create OPRF request";
    case Result::kInvalidOprfReply:
      return "Invalid reply to OPRF request";
    case Result::kCreateQueryRequestFailed:
      return "Failed to create Query request";
    case Result::kInvalidQueryReply:
      return "Invalid reply to Query request";
    case Result::kNoLockConfiguration:
      return "Lock configuration not found";
    case Result::kInvalidConfiguration:
      return "Lock configuration invalid";
    case Result::kLockedWithoutTopic:
      return "Locked configuration without topic";
    case Result::kEmptySignedConfiguration:
      return "Signed configuration not provided";
  }
}

constexpr Result CarrierLockResultToResult(CarrierLockResult result) {
  switch (result) {
    case CarrierLockResult::kSuccess:
      return Result::kSuccess;
    case CarrierLockResult::kUnknownError:
      return Result::kModemInternalError;
    case CarrierLockResult::kInvalidSignature:
      return Result::kInvalidSignature;
    case CarrierLockResult::kInvalidImei:
      return Result::kInvalidImei;
    case CarrierLockResult::kInvalidTimeStamp:
      return Result::kInvalidTimestamp;
    case CarrierLockResult::kNetworkListTooLarge:
      return Result::kNetworkListTooLarge;
    case CarrierLockResult::kAlgorithmNotSupported:
      return Result::kAlgorithmNotSupported;
    case CarrierLockResult::kFeatureNotSupported:
      return Result::kFeatureNotSupported;
    case CarrierLockResult::kDecodeOrParsingError:
      return Result::kDecodeOrParsingError;
    case CarrierLockResult::kNotInitialized:
      return Result::kHandlerNotInitialized;
    case CarrierLockResult::kOperationNotSupported:
      return Result::kOperationNotSupported;
  }
}

}  // namespace

// static
std::unique_ptr<CarrierLockManager> CarrierLockManager::Create(
    PrefService* local_state,
    gcm::GCMDriver* gcm_driver,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(local_state);
  std::unique_ptr<CarrierLockManager> manager =
      std::make_unique<CarrierLockManager>(local_state);

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler* network_handler = NetworkHandler::Get();
    manager->network_state_handler_ = network_handler->network_state_handler();
    manager->modem_handler_ = network_handler->network_3gpp_handler();
  }
  manager->config_ =
      std::make_unique<ProvisioningConfigFetcherImpl>(url_loader_factory);
  manager->psm_ = std::make_unique<PsmClaimVerifierImpl>(url_loader_factory);
  manager->fcm_ = std::make_unique<FcmTopicSubscriberImpl>(
      gcm_driver, kFcmAppId, kFcmSenderId, url_loader_factory);

  manager->Initialize();

  g_instance = manager.get();
  return manager;
}

// static
std::unique_ptr<CarrierLockManager> CarrierLockManager::CreateForTesting(
    PrefService* local_state,
    Network3gppHandler* network_3gpp_handler,
    std::unique_ptr<FcmTopicSubscriber> fcm_topic_subscriber,
    std::unique_ptr<PsmClaimVerifier> psm_claim_verifier,
    std::unique_ptr<ProvisioningConfigFetcher> provisioning_config_fetcher) {
  DCHECK(local_state);
  std::unique_ptr<CarrierLockManager> manager =
      std::make_unique<CarrierLockManager>(local_state);

  manager->network_state_handler_ = nullptr;
  manager->modem_handler_ = network_3gpp_handler;
  manager->config_ = std::move(provisioning_config_fetcher);
  manager->psm_ = std::move(psm_claim_verifier);
  manager->fcm_ = std::move(fcm_topic_subscriber);
  manager->imei_ = "00000000";
  manager->manufacturer_ = "Google";
  manager->model_ = "Pixel 20";
  manager->fcm_->Initialize(
      BindRepeating(&CarrierLockManager::FcmNotification,
                    manager->weak_ptr_factory_.GetWeakPtr()));

  // Start with PSM check.
  manager->RunStep(ConfigurationState::kPsmCheckClaim);

  g_instance = manager.get();
  return manager;
}

// static
void CarrierLockManager::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kDisableManagerPref, false);
  registry->RegisterIntegerPref(kErrorCounterPref, 0);
  registry->RegisterStringPref(kFcmTopicPref, std::string());
  registry->RegisterTimePref(kLastConfigTimePref, base::Time());
  registry->RegisterStringPref(kLastImeiPref, std::string());
  registry->RegisterStringPref(kSignedConfigPref, std::string());
}

// static
ModemLockStatus CarrierLockManager::GetModemLockStatus() {
  if (!g_instance || !g_instance->local_state_) {
    return ModemLockStatus::kUnknown;
  }
  if (g_instance->local_state_->GetBoolean(kDisableManagerPref)) {
    return ModemLockStatus::kNotLocked;
  }
  if (!g_instance->local_state_->GetString(kFcmTopicPref).empty()) {
    return ModemLockStatus::kCarrierLocked;
  }
  return ModemLockStatus::kUnknown;
}

CarrierLockManager::CarrierLockManager(PrefService* local_state)
    : local_state_(local_state), retry_backoff_(&kRetryBackoffPolicy) {}

CarrierLockManager::~CarrierLockManager() {
  g_instance = nullptr;
  if (session_manager_) {
    session_manager_->RemoveObserver(this);
  }
  if (network_state_handler_ && network_state_handler_->HasObserver(this)) {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  }
  if (local_state_) {
    local_state_->SetInteger(kErrorCounterPref, error_counter_);
  }
}

void CarrierLockManager::OnSessionStateChanged() {
  if (!session_manager_) {
    return;
  }

  session_manager::SessionState session_state =
      session_manager_->session_state();
  if (session_state <= session_manager::SessionState::OOBE) {
    // Wait for end of the OOBE.
    return;
  }

  // Once the OOBE is over, disable observer and wait for network connectivity.
  session_manager_->RemoveObserver(this);
  session_manager_ = nullptr;
  trigger_first_run_++;
  network_state_handler_->AddObserver(this, FROM_HERE);
  DefaultNetworkChanged(nullptr);
}

void CarrierLockManager::Initialize() {
  configuration_state_ = ConfigurationState::kNone;
  error_counter_ = local_state_->GetInteger(kErrorCounterPref);

  // Check Disable flag.
  if (local_state_->GetBoolean(kDisableManagerPref)) {
    LOG(WARNING) << "Manager is Disabled by local flag!";
    return;
  }

  // Check for cellular modem and disable Manager if not needed.
  const base::FilePath modem_path = base::FilePath(kFirmwareVariantPath);
  if (!base::PathExists(modem_path)) {
    local_state_->SetBoolean(kDisableManagerPref, true);
    LOG(WARNING) << "No modem found. Manager will be disabled.";
    return;
  }

  // Check handlers.
  if (!network_state_handler_) {
    LOG(ERROR) << "NetworkStateHandler is not initialized.";
    LogError(Result::kInvalidNetworkHandler);
    RunStep(ConfigurationState::kFatalError);
    return;
  }

  if (!modem_handler_) {
    LOG(ERROR) << "Network3gppHandler is not initialized.";
    LogError(Result::kInvalidModemHandler);
    RunStep(ConfigurationState::kFatalError);
    return;
  }

  if (!fcm_ || !psm_ || !config_ ||
      !fcm_->Initialize(BindRepeating(&CarrierLockManager::FcmNotification,
                                      weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Failed to create auxiliary classes.";
    LogError(Result::kInvalidAuxHandlers);
    RunStep(ConfigurationState::kFatalError);
    return;
  }

  // Load manufacturer and model.
  const base::FilePath oem_path(kManufacturerNamePath);
  if (base::PathExists(oem_path)) {
    base::File file(oem_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    int64_t length = file.GetLength();
    base::FixedArray<char> buffer(length + 1);
    UNSAFE_TODO(file.Read(0, buffer.data(), length));
    buffer[length] = '\0';
    manufacturer_ = buffer.data();
  } else {
    LOG(WARNING) << "Manufacturer name file doesn't exist!";
  }

  const base::FilePath model_path(kModelNamePath);
  if (base::PathExists(model_path)) {
    base::File file(model_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    int64_t length = file.GetLength();
    base::FixedArray<char> buffer(length + 1);
    UNSAFE_TODO(file.Read(0, buffer.data(), length));
    buffer[length] = '\0';
    model_ = buffer.data();
  } else {
    LOG(WARNING) << "Model name file doesn't exist!";
  }

  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  retry_backoff_.Reset();

  if (!session_manager_) {
    session_manager_ = session_manager::SessionManager::Get();
    session_manager_->AddObserver(this);
  }
  OnSessionStateChanged();
}

void CarrierLockManager::DefaultNetworkChanged(const NetworkState* network) {
  if (!network) {
    network = network_state_handler_->DefaultNetwork();
  } else {
    trigger_network_++;
  }
  if (retry_backoff_.ShouldRejectRequest()) {
    // Ignore this event.
    VLOG(2) << "Change of default network skipped because of backoff timer.";
    return;
  }
  if (configuration_state_ >= ConfigurationState::kDeviceLocked) {
    // Modem configuration is complete.
    return;
  }
  if (!network || !network->IsConnectedState()) {
    VLOG(2) << "No network connectivity.";
    return;
  }

  if (configuration_state_ == ConfigurationState::kNone) {
    // Ready to start configuration process.
    configuration_state_ = ConfigurationState::kInitialize;
    base::UmaHistogramCounts10000(kInitializationTriggerFirstRun,
                                  trigger_first_run_);
    base::UmaHistogramCounts10000(kInitializationTriggerNetwork,
                                  trigger_network_);
    base::UmaHistogramCounts10000(kInitializationTriggerRetryStep,
                                  trigger_retry_step_);
    base::UmaHistogramCounts10000(kInitializationTriggerScheduler,
                                  trigger_scheduler_);
    RunStep(configuration_state_);
    return;
  }

  retry_backoff_.InformOfRequest(/*succeeded=*/false);
  VLOG(2) << "Network connection was interrupted. Restart setup in "
          << retry_backoff_.GetTimeUntilRelease().InSeconds() << " seconds.";
  trigger_scheduler_++;

  // Call this function after delay to check if configuration process
  // failed and restart it from initial step.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CarrierLockManager::DefaultNetworkChanged,
                     weak_ptr_factory_.GetWeakPtr(), nullptr),
      retry_backoff_.GetTimeUntilRelease());
}

void CarrierLockManager::DevicePropertiesUpdated(const DeviceState* device) {
  if (device->type() != shill::kTypeCellular) {
    return;
  }
  const std::string lock_type = device->sim_lock_type();
  if (!lock_type.empty() && lock_type != kCarrierLockType) {
    // SIM card locked, state of carrier lock is unknown.
    return;
  }

  bool is_manager_enabled = !local_state_->GetBoolean(kDisableManagerPref);
  bool is_modem_configured =
      !local_state_->GetTime(kLastConfigTimePref).is_null();

  if (is_manager_enabled) {
    if (!is_modem_configured) {
      // Configuration is in progress.
      base::UmaHistogramEnumeration(kLockState, LockState::kNotConfigured);
    } else if (lock_type.empty()) {
      // Modem is carrier-locked and SIM is allowed.
      base::UmaHistogramEnumeration(kLockState, LockState::kCompatibleSim);
    } else {
      // Modem is carrier-locked and SIM is blocked.
      base::UmaHistogramEnumeration(kLockState, LockState::kIncompatibleSim);
    }
  } else if (lock_type.empty()) {
    if (is_modem_configured) {
      // Modem was locked and unlocked properly.
      base::UmaHistogramEnumeration(kLockState, LockState::kProperlyUnlocked);
    }
  } else {
    // Modem is locked but manager is already disabled (invalid state).
    base::UmaHistogramEnumeration(kLockState, LockState::kIncorrectlyLocked);
  }
}

void CarrierLockManager::RunStep(ConfigurationState state) {
  VLOG(2) << "Run step " << ConfigurationStateToStringView(state);

  switch (state) {
    case ConfigurationState::kInitialize:
      function_ = &CarrierLockManager::CheckState;
      break;
    case ConfigurationState::kPsmCheckClaim:
      function_ = &CarrierLockManager::CheckPsmClaim;
      break;
    case ConfigurationState::kFcmGetToken:
      function_ = &CarrierLockManager::GetFcmToken;
      break;
    case ConfigurationState::kRequestConfig:
      function_ = &CarrierLockManager::RequestConfig;
      break;
    case ConfigurationState::kSetupModem:
      function_ = &CarrierLockManager::SetupModem;
      break;
    case ConfigurationState::kFcmCheckTopic:
      function_ = &CarrierLockManager::CheckFcmTopic;
      break;
    case ConfigurationState::kFcmSubscribe:
      function_ = &CarrierLockManager::SubscribeFcmTopic;
      break;
    case ConfigurationState::kDeviceUnlocked:
      local_state_->SetBoolean(kDisableManagerPref, true);
      // DevMode will be unlocked during next OOBE based on PSM status.
      [[fallthrough]];
    case ConfigurationState::kDeviceLocked:
      error_counter_ = 0;
      // Nothing to do, wait for FCM notification.
      [[fallthrough]];
    case ConfigurationState::kFatalError:
      function_ = nullptr;
      break;
    case ConfigurationState::kNone:
      return;
  }
  configuration_state_ = state;
  remaining_retries_ = kMaxRetries;

  if (function_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(function_, weak_ptr_factory_.GetWeakPtr()));
  }
}

bool CarrierLockManager::RetryStep() {
  if (!function_) {
    return false;
  }

  if (remaining_retries_ <= 0) {
    LOG(ERROR) << "Step "
               << ConfigurationStateToStringView(configuration_state_)
               << " failed " << (kMaxRetries + 1) << " times. Exiting...";
    trigger_retry_step_++;

    // Wait for new connection and retry...
    configuration_state_ = ConfigurationState::kNone;
    return false;
  }
  remaining_retries_--;

  VLOG(2) << "Step " << ConfigurationStateToStringView(configuration_state_)
          << " failed, trying again...";

  // Retry current configuration step after delay.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(function_, weak_ptr_factory_.GetWeakPtr()),
      kRetryDelay);
  return true;
}

void CarrierLockManager::LogError(Result result) {
  VLOG(2) << "Step " << ConfigurationStateToStringView(configuration_state_)
          << " returned error " << ResultToStringView(result);

  switch (configuration_state_) {
    case ConfigurationState::kNone:
    case ConfigurationState::kInitialize:
      base::UmaHistogramEnumeration(kErrorManagerInitialization, result);
      break;
    case ConfigurationState::kPsmCheckClaim:
      base::UmaHistogramEnumeration(kErrorPsmClaim, result);
      break;
    case ConfigurationState::kFcmGetToken:
    case ConfigurationState::kFcmSubscribe:
      base::UmaHistogramEnumeration(kErrorFcmTopic, result);
      break;
    case ConfigurationState::kRequestConfig:
      base::UmaHistogramEnumeration(kErrorProvisioning, result);
      break;
    case ConfigurationState::kSetupModem:
      base::UmaHistogramEnumeration(kErrorModemSetup, result);
      break;
    default:
      break;
  }

  local_state_->SetInteger(kErrorCounterPref, ++error_counter_);
  if (error_counter_ && !(error_counter_ % 10)) {
    base::UmaHistogramCounts1M(kNumConsecutiveConfigurationFailures,
                               error_counter_);
  }
}

void CarrierLockManager::CheckState() {
  if (serial_.empty()) {
    system::StatisticsProvider* statistics =
        system::StatisticsProvider::GetInstance();
    if (!statistics) {
      LOG(ERROR) << "StatisticsProvider is not initialized.";
      LogError(Result::kSerialProviderFailed);
      RetryStep();
      return;
    }

    serial_ = statistics->GetMachineID().value_or(std::string());
    if (serial_.empty()) {
      LOG(ERROR) << "Failed to read Serial number";
      LogError(Result::kSerialProviderFailed);
      RetryStep();
      return;
    }

    // Overwrite oem and model if defined in VPD.
    if (const std::optional<std::string_view> model =
            statistics->GetMachineStatistic(kMachineModelName)) {
      model_ = model.value();
      LOG(WARNING) << "Model changed to " << model_ << ".";
    }
    if (const std::optional<std::string_view> oem =
            statistics->GetMachineStatistic(kMachineOemName)) {
      manufacturer_ = oem.value();
      LOG(WARNING) << "Manufacturer changed to " << manufacturer_ << ".";
    }
  }

  // First configuration, check PSM claim.
  base::Time last_config = local_state_->GetTime(kLastConfigTimePref);
  if (last_config.is_null()) {
    base::UmaHistogramEnumeration(kConfigurationStateAfterInitialization,
                                  InitialState::kFirstConfiguration);
    is_first_setup_ = true;
    RunStep(ConfigurationState::kPsmCheckClaim);
    return;
  }

  // Configuration older than 30 days, get FCM token and new configuration.
  if (base::Time::Now() - last_config >= kFcmTimeout) {
    base::UmaHistogramEnumeration(kConfigurationStateAfterInitialization,
                                  InitialState::kObsoleteConfiguration);
    is_first_setup_ = false;
    RunStep(ConfigurationState::kFcmGetToken);
    return;
  }

  // Get IMEI of cellular modem.
  const DeviceState* cellular_device =
      network_state_handler_->GetDeviceStateByType(
          ash::NetworkTypePattern::Cellular());
  if (!cellular_device) {
    LOG(ERROR) << "Cellular device not found or invalid.";
    LogError(Result::kModemNotFound);
    RetryStep();
    return;
  }

  imei_ = cellular_device->imei();
  if (imei_.empty()) {
    LOG(ERROR) << "Cellular device has invalid IMEI.";
    LogError(Result::kInvalidImei);
    RetryStep();
    return;
  }

  // Apply stored configuration if possible or request new token and config.
  std::string last_imei = local_state_->GetString(kLastImeiPref);
  std::string signed_config = local_state_->GetString(kSignedConfigPref);
  std::string fcm_topic = local_state_->GetString(kFcmTopicPref);
  if ((imei_ == last_imei) && !signed_config.empty() && !fcm_topic.empty()) {
    base::UmaHistogramEnumeration(kConfigurationStateAfterInitialization,
                                  InitialState::kAlreadyConfigured);
    is_first_setup_ = false;
    RunStep(ConfigurationState::kSetupModem);
  } else {
    is_first_setup_ = (imei_ != last_imei);
    base::UmaHistogramEnumeration(
        kConfigurationStateAfterInitialization,
        (is_first_setup_ ? InitialState::kModemImeiChanged
                         : InitialState::kEmptySignedConfig));
    RunStep(ConfigurationState::kFcmGetToken);
  }
}

void CarrierLockManager::CheckPsmClaim() {
  psm_->CheckPsmClaim(serial_, base::ToLowerASCII(manufacturer_),
                      base::ToLowerASCII(model_),
                      base::BindOnce(&CarrierLockManager::PsmCallback,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void CarrierLockManager::PsmCallback(Result result) {
  if (result != Result::kSuccess) {
    LogError(result);
    if (result != Result::kHandlerBusy) {
      RetryStep();
    }
    return;
  }

  if (psm_->GetMembership()) {
    base::UmaHistogramEnumeration(kPsmClaimResponse, PsmResult::kDeviceLocked);
    RunStep(ConfigurationState::kFcmGetToken);
  } else {
    LOG(WARNING) << "Not a member in PSM group, manager will be disabled.";
    base::UmaHistogramEnumeration(kPsmClaimResponse,
                                  PsmResult::kDeviceUnlocked);
    RunStep(ConfigurationState::kDeviceUnlocked);
  }
}

void CarrierLockManager::RequestConfig() {
  if (imei_.empty()) {
    const DeviceState* cellular_device =
        network_state_handler_->GetDeviceStateByType(
            ash::NetworkTypePattern::Cellular());
    if (!cellular_device) {
      LOG(ERROR) << "Cellular device not found or invalid.";
      LogError(Result::kModemNotFound);
      RetryStep();
      return;
    }

    imei_ = cellular_device->imei();
    if (imei_.empty()) {
      LOG(ERROR) << "Cellular device has invalid IMEI.";
      LogError(Result::kInvalidImei);
      RetryStep();
      return;
    }
  }

  config_->RequestConfig(serial_, imei_, manufacturer_, model_, fcm_token_,
                         base::BindOnce(&CarrierLockManager::ConfigCallback,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CarrierLockManager::ConfigCallback(Result result) {
  if (result != Result::kSuccess) {
    LogError(result);
    if (result != Result::kHandlerBusy) {
      RetryStep();
    }
    return;
  }

  RestrictedNetworks networks = config_->GetNumberOfNetworks();
  bool is_config_unlocked = !networks.allowed && !networks.disallowed;
  if (config_->GetFcmTopic().empty()) {
    // Unlocked or Invalid configuration.
    base::UmaHistogramEnumeration(kProvisioningServerResponse,
                                  is_config_unlocked
                                      ? ProvisioningResult::kConfigUnlocked
                                      : ProvisioningResult::kConfigInvalid);
    if (!is_config_unlocked) {
      // Invalid config (locked but without fcm topic).
      LogError(Result::kLockedWithoutTopic);
      RetryStep();
      return;
    }
  } else {
    // Locked or Temporarily unlocked configuration.
    base::UmaHistogramEnumeration(kProvisioningServerResponse,
                                  is_config_unlocked
                                      ? ProvisioningResult::kConfigTempUnlocked
                                      : ProvisioningResult::kConfigLocked);
  }

  // Check signed configuration and store it.
  if (config_->GetSignedConfig().empty()) {
    LogError(Result::kEmptySignedConfiguration);
    RetryStep();
    return;
  }
  std::string signed_config = base::Base64Encode(config_->GetSignedConfig());
  local_state_->SetString(kSignedConfigPref, signed_config);

  if (!local_state_->GetString(kFcmTopicPref).empty() &&
      config_->GetFcmTopic().empty()) {
    NetworkConnect::Get()->ShowCarrierUnlockNotification();
  }

  // Save current time, modem imei and fcm topic.
  local_state_->SetString(kFcmTopicPref, config_->GetFcmTopic());
  local_state_->SetTime(kLastConfigTimePref, base::Time::Now());
  local_state_->SetString(kLastImeiPref, imei_);

  RunStep(ConfigurationState::kSetupModem);
}

void CarrierLockManager::SetupModem() {
  std::string signed_config;
  base::Base64Decode(local_state_->GetString(kSignedConfigPref),
                     &signed_config);

  // Make sure the manager is enabled before locking the modem.
  if (!local_state_->GetString(kFcmTopicPref).empty() &&
      local_state_->GetBoolean(kDisableManagerPref)) {
    local_state_->SetBoolean(kDisableManagerPref, false);
  }

  modem_handler_->SetCarrierLock(
      signed_config, base::BindOnce(&CarrierLockManager::SetupModemCallback,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CarrierLockManager::SetupModemCallback(CarrierLockResult result) {
  // Ignore InvalidTimestamp, it is returned when the config was applied again.
  if ((result != CarrierLockResult::kSuccess) &&
      (result != CarrierLockResult::kInvalidTimeStamp)) {
    LogError(CarrierLockResultToResult(result));
    RetryStep();
    return;
  }

  if (local_state_->GetString(kFcmTopicPref).empty()) {
    // Configuration not locked.
    base::UmaHistogramEnumeration(kModemConfigurationResult,
                                  is_first_setup_
                                      ? ConfigurationResult::kModemNotLocked
                                      : ConfigurationResult::kModemUnlocked);
  } else {
    // Configuration carrier-locked.
    base::UmaHistogramEnumeration(kModemConfigurationResult,
                                  is_first_setup_
                                      ? ConfigurationResult::kModemLocked
                                      : ConfigurationResult::kModemRelocked);
  }
  is_first_setup_ = false;

  RunStep(ConfigurationState::kFcmCheckTopic);
}

void CarrierLockManager::GetFcmToken() {
  fcm_->RequestToken(BindOnce(&CarrierLockManager::FcmTokenCallback,
                              weak_ptr_factory_.GetWeakPtr()));
}

void CarrierLockManager::FcmTokenCallback(Result result) {
  if (result != Result::kSuccess) {
    LogError(result);
    if (result != Result::kHandlerBusy) {
      RetryStep();
    }
    return;
  }

  fcm_token_ = fcm_->token();

  base::UmaHistogramEnumeration(kFcmCommunicationResult,
                                FcmResult::kRegistered);

  RunStep(ConfigurationState::kRequestConfig);
}

void CarrierLockManager::CheckFcmTopic() {
  if (local_state_->GetString(kFcmTopicPref).empty()) {
    VLOG(2) << "FCM topic not provided with config, modem was unlocked.";
    base::UmaHistogramCounts1M(kNumConsecutiveFailuresBeforeUnlock,
                               error_counter_);
    RunStep(ConfigurationState::kDeviceUnlocked);
    return;
  }

  base::UmaHistogramCounts1M(kNumConsecutiveFailuresBeforeLock, error_counter_);
  RunStep(ConfigurationState::kFcmSubscribe);
}

void CarrierLockManager::SubscribeFcmTopic() {
  std::string fcm_topic = local_state_->GetString(kFcmTopicPref);

  fcm_->SubscribeTopic(fcm_topic,
                       BindOnce(&CarrierLockManager::FcmTopicCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void CarrierLockManager::FcmTopicCallback(Result result) {
  if (result != Result::kSuccess) {
    LogError(result);
    if (result != Result::kHandlerBusy) {
      RetryStep();
    }
    return;
  }

  base::UmaHistogramEnumeration(kFcmCommunicationResult,
                                FcmResult::kSubscribed);
  RunStep(ConfigurationState::kDeviceLocked);
}

void CarrierLockManager::FcmNotification(bool is_from_topic) {
  // Set LastConfigTime to value older than FCM timeout (usually 30 days)
  // in order to request new configuration in case of failure or reboot.
  local_state_->SetTime(kLastConfigTimePref, base::Time::Now() - kFcmTimeout);

  if (configuration_state_ == ConfigurationState::kNone) {
    base::UmaHistogramEnumeration(
        kFcmNotificationType,
        (is_from_topic ? FcmNotification::kUpdateProfileBeforeInit
                       : FcmNotification::kUnlockDeviceBeforeInit));
    // Configuration process will start from beginning.
    return;
  }
  if (configuration_state_ < ConfigurationState::kDeviceLocked) {
    base::UmaHistogramEnumeration(
        kFcmNotificationType,
        (is_from_topic ? FcmNotification::kUpdateProfileWhileBusy
                       : FcmNotification::kUnlockDeviceWhileBusy));

    // Wait until the configuration process is completed.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CarrierLockManager::FcmNotification,
                       weak_ptr_factory_.GetWeakPtr(), is_from_topic),
        kConfigDelay);
    return;
  }

  base::UmaHistogramEnumeration(
      kFcmNotificationType, (is_from_topic ? FcmNotification::kUpdateProfile
                                           : FcmNotification::kUnlockDevice));

  // Call to RunStep is delayed to workaround racing issue: b/265987223
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CarrierLockManager::RunStep,
                     weak_ptr_factory_.GetWeakPtr(),
                     ConfigurationState::kRequestConfig),
      kConfigDelay);
}

}  // namespace ash::carrier_lock
