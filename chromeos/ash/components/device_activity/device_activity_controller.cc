// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/device_activity_controller.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/device_activity/churn_cohort_use_case_impl.h"
#include "chromeos/ash/components/device_activity/churn_observation_use_case_impl.h"
#include "chromeos/ash/components/device_activity/daily_use_case_impl.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "chromeos/ash/components/device_activity/device_activity_client.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/twenty_eight_day_active_use_case_impl.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {
DeviceActivityController* g_ash_device_activity_controller = nullptr;

// Policy device modes that should be classified as not being set.
static const std::unordered_set<policy::DeviceMode>& DeviceModeNotSet() {
  static const std::unordered_set<policy::DeviceMode> kModeNotSet(
      {policy::DeviceMode::DEVICE_MODE_PENDING,
       policy::DeviceMode::DEVICE_MODE_NOT_SET});
  return kModeNotSet;
}

// Policy device modes that should be classified as consumer devices.
static const std::unordered_set<policy::DeviceMode>& DeviceModeConsumer() {
  static const std::unordered_set<policy::DeviceMode> kModeConsumer(
      {policy::DeviceMode::DEVICE_MODE_CONSUMER,
       policy::DeviceMode::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH});
  return kModeConsumer;
}

// Policy device modes that should be classified as enterprise devices.
static const std::unordered_set<policy::DeviceMode>& DeviceModeEnterprise() {
  static const std::unordered_set<policy::DeviceMode> kModeEnterprise(
      {policy::DeviceMode::DEVICE_MODE_ENTERPRISE,
       policy::DeviceMode::DEVICE_MODE_ENTERPRISE_AD,
       policy::DeviceMode::DEVICE_MODE_DEMO});
  return kModeEnterprise;
}

// Production edge server for reporting device actives.
// TODO(https://crbug.com/1267432): Enable passing base url as a runtime flag.
const char kFresnelBaseUrl[] = "https://crosfresnel-pa.googleapis.com";

// Number of minutes to wait before retrying
// reading the .oobe_completed file again.
constexpr base::TimeDelta kOobeReadFailedRetryDelay = base::Minutes(60);

// Number of times to retry before failing to report any device actives.
constexpr int kNumberOfRetriesBeforeFail = 120;

// Count the number of PSM device active secret that is set.
const char kDeviceActiveControllerPsmDeviceActiveSecretIsSet[] =
    "Ash.DeviceActiveController.PsmDeviceActiveSecretIsSet";

// Count the number of devices that are testimage builds.
const char kDeviceActiveControllerIsTestImageDevice[] =
    "Ash.DeviceActiveController.IsTestImageDevice";

void RecordPsmDeviceActiveSecretIsSet(bool is_set) {
  base::UmaHistogramBoolean(kDeviceActiveControllerPsmDeviceActiveSecretIsSet,
                            is_set);
}

void RecordIsTestImageDevice(bool is_test_image) {
  base::UmaHistogramBoolean(kDeviceActiveControllerIsTestImageDevice,
                            is_test_image);
}

class PsmDelegateImpl : public PsmDelegateInterface {
 public:
  PsmDelegateImpl() = default;
  PsmDelegateImpl(const PsmDelegateImpl&) = delete;
  PsmDelegateImpl& operator=(const PsmDelegateImpl&) = delete;
  ~PsmDelegateImpl() override = default;

  // PsmDelegateInterface:
  rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(
      psm_rlwe::RlweUseCase use_case,
      const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) override {
    return psm_rlwe::PrivateMembershipRlweClient::Create(use_case,
                                                         plaintext_ids);
  }
};

}  // namespace

DeviceActivityController* DeviceActivityController::Get() {
  return g_ash_device_activity_controller;
}

// static
void DeviceActivityController::RegisterPrefs(PrefRegistrySimple* registry) {
  const base::Time unix_epoch = base::Time::UnixEpoch();
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(
      prefs::kDeviceActiveLastKnown28DayActivePingTimestamp, unix_epoch);
  registry->RegisterTimePref(
      prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp, unix_epoch);
  registry->RegisterTimePref(
      prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp, unix_epoch);
  registry->RegisterIntegerPref(prefs::kDeviceActiveLastKnownChurnActiveStatus,
                                0);
  registry->RegisterBooleanPref(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, false);
  registry->RegisterBooleanPref(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, false);
  registry->RegisterBooleanPref(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, false);
}

// static
base::TimeDelta DeviceActivityController::DetermineStartUpDelay(
    base::Time chrome_first_run_ts) {
  // Wait at least 10 minutes from the first chrome run sentinel file creation
  // time. This creation time is used as an indicator of when the device last
  // reset (powerwashed/recovery/RMA). PSM servers take 10 minutes from CheckIn
  // to return the correct response for CheckMembership requests, since the PSM
  // servers need to update their cache.
  //
  // This delay avoids the scenario where a device checks in, powerwashes, and
  // on device start up, gets the wrong check membership response.
  base::TimeDelta delay_on_first_chrome_run;
  base::Time current_ts = base::Time::Now();
  if (current_ts < (chrome_first_run_ts + base::Minutes(10))) {
    delay_on_first_chrome_run =
        chrome_first_run_ts + base::Minutes(10) - current_ts;
  }

  return delay_on_first_chrome_run;
}

// static
MarketSegment DeviceActivityController::GetMarketSegment(
    policy::DeviceMode device_mode,
    policy::MarketSegment device_market_segment) {
  // Determine Fresnel market segment using the retrieved device policy
  // |device_mode| and |device_market_segment|.
  if (DeviceModeNotSet().count(device_mode)) {
    return MARKET_SEGMENT_UNKNOWN;
  }

  if (DeviceModeConsumer().count(device_mode)) {
    return MARKET_SEGMENT_CONSUMER;
  }

  if (DeviceModeEnterprise().count(device_mode)) {
    if (device_market_segment == policy::MarketSegment::ENTERPRISE) {
      return MARKET_SEGMENT_ENTERPRISE;
    }

    if (device_market_segment == policy::MarketSegment::EDUCATION) {
      return MARKET_SEGMENT_EDUCATION;
    }

    return MARKET_SEGMENT_ENTERPRISE_ENROLLED_BUT_UNKNOWN;
  }

  return MARKET_SEGMENT_UNKNOWN;
}

DeviceActivityController::DeviceActivityController(
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Time chrome_first_run_time,
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback)
    : chrome_first_run_time_(chrome_first_run_time),
      chrome_passed_device_params_(chrome_passed_device_params),
      statistics_provider_(system::StatisticsProvider::GetInstance()),
      oobe_completed_timer_(std::make_unique<base::OneShotTimer>()) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerConstructor);

  DCHECK(local_state);
  DCHECK(!g_ash_device_activity_controller);

  g_ash_device_activity_controller = this;

  // Halt if device is a testimage/unknown channel.
  if (chrome_passed_device_params.chromeos_channel ==
      version_info::Channel::UNKNOWN) {
    RecordIsTestImageDevice(true);
    LOG(ERROR) << "Halt - Client should enter device active reporting logic. "
               << "Unknown and test image channels should not be counted as "
               << "legitimate device counts.";
    return;
  } else {
    RecordIsTestImageDevice(false);
  }

  // Check if active status value is set in local state. If not set, we will
  // attempt to restore from preserved file in the device activity client.
  // If set, override the |churn_active_status_|. If both layers of caching
  // is empty, we will perform check membership requests on the cohort requests
  // (contains active status objects) to determine the last known value.
  int churn_active_value =
      local_state->GetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus);
  if (churn_active_value == 0) {
    LOG(ERROR) << "Active status is not set in the local state.";
    LOG(ERROR) << "Setting value for |churn_active_status_ptr_| to 0.";
  }

  churn_active_status_.InitializeValue(churn_active_value);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&device_activity::DeviceActivityController::Start,
                     weak_factory_.GetWeakPtr(), local_state,
                     url_loader_factory,
                     std::move(check_oobe_completed_callback)),
      DeviceActivityController::DetermineStartUpDelay(chrome_first_run_time));
}

DeviceActivityController::~DeviceActivityController() {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerDestructor);
  DCHECK_EQ(this, g_ash_device_activity_controller);
  Stop();
  g_ash_device_activity_controller = nullptr;
}

int DeviceActivityController::GetRetryOobeCompletedCountForTesting() const {
  return retry_oobe_completed_count_;
}

base::OneShotTimer*
DeviceActivityController::GetOobeCompletedTimerForTesting() {
  DCHECK(oobe_completed_timer_.get() != nullptr);
  return oobe_completed_timer_.get();
}

void DeviceActivityController::Start(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerStart);

  CheckOobeCompletedInWorker(local_state, url_loader_factory,
                             std::move(check_oobe_completed_callback));
}

void DeviceActivityController::Stop() {
  if (da_client_network_) {
    da_client_network_.reset();
  }
}

void DeviceActivityController::CheckOobeCompletedInWorker(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(check_oobe_completed_callback),
      base::BindOnce(&DeviceActivityController::OnOobeFileWritten,
                     weak_factory_.GetWeakPtr(), local_state,
                     url_loader_factory, check_oobe_completed_callback));
}

void DeviceActivityController::OnOobeFileWritten(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback,
    base::TimeDelta time_since_oobe_file_written) {
  // We block if the oobe completed file is not written.
  // ChromeOS devices should go through oobe to be considered a real device.
  // The ActivateDate is also only set after oobe is written.
  if (retry_oobe_completed_count_ >= kNumberOfRetriesBeforeFail) {
    LOG(ERROR) << "Retry failed - .oobe_completed file was not written for "
               << "1 minute after retrying 120 times. "
               << "There was a 60 minute wait between each retry and spanned "
               << "5 days.";
    return;
  }

  if (time_since_oobe_file_written < base::Minutes(1)) {
    retry_oobe_completed_count_ += 1;

    LOG(ERROR) << "Time since oobe file created was less than 1 minute. "
               << std::endl
               << "Wait and retry again after 1 minute to ensure that "
               << "the ActivateDate VPD field is set. " << std::endl
               << "TimeDelta since oobe flag file was created = "
               << time_since_oobe_file_written
               << ". Retry count = " << retry_oobe_completed_count_;

    oobe_completed_timer_->Start(
        FROM_HERE, kOobeReadFailedRetryDelay,
        base::BindOnce(&DeviceActivityController::CheckOobeCompletedInWorker,
                       weak_factory_.GetWeakPtr(), local_state,
                       url_loader_factory,
                       std::move(check_oobe_completed_callback)));

    return;
  }

  // Wrap with callback from |psm_device_active_secret_| retrieval using
  // |SessionManagerClient| DBus.
  SessionManagerClient::Get()->GetPsmDeviceActiveSecret(base::BindOnce(
      &device_activity::DeviceActivityController::
          OnPsmDeviceActiveSecretFetched,
      weak_factory_.GetWeakPtr(), local_state, url_loader_factory));
}

void DeviceActivityController::OnPsmDeviceActiveSecretFetched(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& psm_device_active_secret) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerOnPsmDeviceActiveSecretFetched);

  // In order for the device actives to be reported, the psm device active
  // secret must be retrieved successfully.
  if (psm_device_active_secret.empty()) {
    RecordPsmDeviceActiveSecretIsSet(false);
    VLOG(1) << "Can not generate PSM id without the psm device secret "
               "being defined.";
    return;
  }

  RecordPsmDeviceActiveSecretIsSet(true);

  // Continue when machine statistics are loaded, to avoid blocking.
  statistics_provider_->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
      &device_activity::DeviceActivityController::OnMachineStatisticsLoaded,
      weak_factory_.GetWeakPtr(), local_state, url_loader_factory,
      psm_device_active_secret));
}

void DeviceActivityController::OnMachineStatisticsLoaded(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& psm_device_active_secret) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerOnMachineStatisticsLoaded);

  std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases;
  use_cases.push_back(std::make_unique<DailyUseCaseImpl>(
      psm_device_active_secret, chrome_passed_device_params_, local_state,
      std::make_unique<PsmDelegateImpl>()));
  use_cases.push_back(std::make_unique<TwentyEightDayActiveUseCaseImpl>(
      psm_device_active_secret, chrome_passed_device_params_, local_state,
      std::make_unique<PsmDelegateImpl>()));
  use_cases.push_back(std::make_unique<ChurnCohortUseCaseImpl>(
      &churn_active_status_, psm_device_active_secret,
      chrome_passed_device_params_, local_state,
      std::make_unique<PsmDelegateImpl>()));
  use_cases.push_back(std::make_unique<ChurnObservationUseCaseImpl>(
      &churn_active_status_, psm_device_active_secret,
      chrome_passed_device_params_, local_state,
      std::make_unique<PsmDelegateImpl>()));

  da_client_network_ = std::make_unique<DeviceActivityClient>(
      &churn_active_status_, local_state,
      NetworkHandler::Get()->network_state_handler(), url_loader_factory,
      std::make_unique<base::RepeatingTimer>(), kFresnelBaseUrl,
      google_apis::GetFresnelAPIKey(), chrome_first_run_time_,
      std::move(use_cases));
}

}  // namespace ash::device_activity
