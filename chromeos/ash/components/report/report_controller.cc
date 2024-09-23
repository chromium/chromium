// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/report_controller.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/report/device_metrics/actives/one_day_impl.h"
#include "chromeos/ash/components/report/device_metrics/actives/twenty_eight_day_impl.h"
#include "chromeos/ash/components/report/device_metrics/churn/cohort_impl.h"
#include "chromeos/ash/components/report/device_metrics/churn/observation_impl.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/utils/pref_utils.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report {

namespace {

ReportController* g_ash_report_controller = nullptr;

// Amount of time to wait before triggering repeating timer.
constexpr base::TimeDelta kTimeToRepeat = base::Hours(1);

// Maximum time to wait for time sync before not reporting device as active
// in current attempt.
// This corresponds to at least seven TCP retransmissions attempts to
// the remote server used to update the system clock.
constexpr base::TimeDelta kSystemClockSyncWaitTimeout = base::Seconds(45);

// Create helper methods for lambda functions.
base::OnceClosure CreateSavePreservedFileCallback(
    PrefService* local_state,
    base::WeakPtr<ReportController> controller_weak_ptr) {
  return base::BindOnce(
      [](PrefService* local_state,
         base::WeakPtr<ReportController> controller_weak_ptr) {
        PrivateComputingClient::Get()->SaveLastPingDatesStatus(
            utils::CreatePreservedFileContents(local_state),
            base::BindOnce(
                &ReportController::OnSaveLocalStateToPreservedFileComplete,
                controller_weak_ptr));
      },
      local_state, controller_weak_ptr);
}

base::OnceClosure CreateReportObservationCallback(
    base::WeakPtr<device_metrics::ObservationImpl> observation_weak_ptr,
    base::OnceCallback<void()> save_preserved_file_cb) {
  return base::BindOnce(&device_metrics::ObservationImpl::Run,
                        observation_weak_ptr,
                        std::move(save_preserved_file_cb));
}

base::OnceClosure CreateReportCohortCallback(
    base::WeakPtr<device_metrics::CohortImpl> cohort_weak_ptr,
    base::OnceCallback<void()> report_observation_cb) {
  return base::BindOnce(&device_metrics::CohortImpl::Run, cohort_weak_ptr,
                        std::move(report_observation_cb));
}

base::OnceClosure CreateReport28DaCallback(
    base::WeakPtr<device_metrics::TwentyEightDayImpl> twenty_eight_day_weak_ptr,
    base::OnceCallback<void()> report_cohort_cb) {
  return base::BindOnce(&device_metrics::TwentyEightDayImpl::Run,
                        twenty_eight_day_weak_ptr, std::move(report_cohort_cb));
}

// Record boolean indicating the preserved file has been written successfully.
void RecordPreservedFileWritten(bool is_written) {
  base::UmaHistogramBoolean("Ash.Report.PreservedFileWritten", is_written);
}

// Record boolean for whether the device is classified as for testing.
void RecordIsTestDevice(bool is_test) {
  base::UmaHistogramBoolean("Ash.Report.IsTestDevice", is_test);
}

// Record boolean for whether the psm stable secret key is read and parsed
// successfully from the VPD file that is loaded to machine statistics.
void RecordIsPsmSecretSet(bool is_set) {
  base::UmaHistogramBoolean("Ash.Report.IsPsmSecretSet", is_set);
}

// Record boolean for if the system clock has been synced successfully or not.
void RecordIsSystemClockSynced(bool is_synced) {
  base::UmaHistogramBoolean("Ash.Report.IsSystemClockSynced", is_synced);
}

}  // namespace

ReportController* ReportController::Get() {
  return g_ash_report_controller;
}

// static
void ReportController::RegisterPrefs(PrefRegistrySimple* registry) {
  const base::Time unix_epoch = base::Time::UnixEpoch();
  registry->RegisterTimePref(
      prefs::kDeviceActiveLastKnown1DayActivePingTimestamp, unix_epoch);
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

  registry->RegisterDictionaryPref(prefs::kDeviceActive28DayActivePingCache);
}

ReportController::ReportController(
    const device_metrics::ChromeDeviceMetadataParameters& chrome_device_params,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<device_metrics::PsmClientManager> psm_client_manager)
    : chrome_device_params_(chrome_device_params),
      local_state_(local_state),
      url_loader_factory_(url_loader_factory),
      report_timer_(std::make_unique<base::RepeatingTimer>()),
      network_state_handler_(NetworkHandler::Get()->network_state_handler()),
      statistics_provider_(system::StatisticsProvider::GetInstance()),
      clock_(base::DefaultClock::GetInstance()),
      psm_client_manager_(std::move(psm_client_manager)) {
  DCHECK(local_state);
  DCHECK(psm_client_manager_.get());
  DCHECK(!g_ash_report_controller);

  g_ash_report_controller = this;

  // Halt if device is a testimage/unknown channel.
  if (chrome_device_params.chrome_channel == version_info::Channel::UNKNOWN) {
    LOG(ERROR) << "Halt - Client should enter device active reporting logic. "
               << "Unknown and test image channels should not be counted as "
               << "legitimate device counts.";
    return;
  }

  // Wrap with callback from |psm_device_active_secret_| retrieval using
  // |SessionManagerClient| DBus.
  SessionManagerClient::Get()->GetPsmDeviceActiveSecret(
      base::BindOnce(&report::ReportController::OnPsmDeviceActiveSecretFetched,
                     weak_factory_.GetWeakPtr()));
}

ReportController::~ReportController() {
  DCHECK_EQ(this, g_ash_report_controller);
  g_ash_report_controller = nullptr;

  // Reset all dependency unique_ptr objects of this class that are not needed.
  report_timer_.reset();
  system_clock_sync_observation_.reset();

  // Reset all reporting use cases.
  one_day_impl_.reset();
  twenty_eight_day_impl_.reset();
  cohort_impl_.reset();
  observation_impl_.reset();

  // Reset parameters that is passed to reporting use cases.
  use_case_params_.reset();
}

void ReportController::DefaultNetworkChanged(const NetworkState* network) {
  bool was_connected = network_connected_;
  network_connected_ = network && network->IsOnline();

  if (network_connected_ == was_connected) {
    return;
  }

  if (network_connected_) {
    OnNetworkOnline();
  } else {
    OnNetworkOffline();
  }
}

void ReportController::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void ReportController::OnPsmDeviceActiveSecretFetched(
    const std::string& psm_device_active_secret) {
  // In order for the device actives to be reported, the psm device active
  // secret must be retrieved successfully.
  if (psm_device_active_secret.empty()) {
    LOG(ERROR) << "PSM device secret is empty and could not be fetched.";
    RecordIsPsmSecretSet(false);
    return;
  }

  RecordIsPsmSecretSet(true);
  high_entropy_seed_ = psm_device_active_secret;

  // Continue when machine statistics are loaded, to avoid blocking.
  statistics_provider_->ScheduleOnMachineStatisticsLoaded(
      base::BindOnce(&report::ReportController::OnMachineStatisticsLoaded,
                     weak_factory_.GetWeakPtr()));
}

void ReportController::OnMachineStatisticsLoaded() {
  // Block virtual machines and debug builds (dev mode enabled).
  if (statistics_provider_->IsRunningOnVm() ||
      statistics_provider_->IsCrosDebugMode()) {
    LOG(ERROR) << "Terminate - device is running in VM or with cros_debug mode "
                  "enabled.";
    LOG(ERROR) << "VM = " << statistics_provider_->IsRunningOnVm()
               << " and DEBUG = " << statistics_provider_->IsCrosDebugMode();
    RecordIsTestDevice(true);
    return;
  }

  RecordIsTestDevice(false);

  // Send DBus method to that reads preserved files for missing local state
  // prefs.
  PrivateComputingClient::Get()->GetLastPingDatesStatus(
      base::BindOnce(&ReportController::OnPreservedFileReadComplete,
                     weak_factory_.GetWeakPtr()));
}

void ReportController::OnPreservedFileReadComplete(
    private_computing::GetStatusResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to read preserved file. "
               << "Error from DBus: " << response.error_message();

    OnReadyToReport();
    return;
  }

  utils::RestoreLocalStateWithPreservedFile(local_state_, response);
  OnReadyToReport();
}

void ReportController::OnSaveLocalStateToPreservedFileComplete(
    private_computing::SaveStatusResponse response) {
  bool write_success = true;
  if (response.has_error_message()) {
    write_success = false;
    LOG(ERROR) << "Failed to write to preserved file. "
               << "Error from DBus: " << response.error_message();
  }
  RecordPreservedFileWritten(write_success);

  // Device is done reporting after writing to preserved file.
  is_device_reporting_ = false;

  // Reset all reporting use cases.
  one_day_impl_.reset();
  twenty_eight_day_impl_.reset();
  cohort_impl_.reset();
  observation_impl_.reset();

  // Reset parameters that is passed to reporting use cases.
  use_case_params_.reset();
}

bool ReportController::IsDeviceReportingForTesting() const {
  return is_device_reporting_;
}

void ReportController::OnReadyToReport() {
  // Retry reporting metrics every 1 hour.
  report_timer_->Start(FROM_HERE, kTimeToRepeat, this,
                       &ReportController::ReportingTriggeredByTimer);

  // Retry reporting metrics if network comes online.
  // DefaultNetworkChanged method is called every time the
  // device network properties change.
  network_state_handler_observer_.Observe(network_state_handler_.get());

  DefaultNetworkChanged(network_state_handler_->DefaultNetwork());
}

void ReportController::ReportingTriggeredByTimer() {
  // Return early if the device is not connected to the network
  // or if it is already in the middle of reporting.
  if (!network_connected_ || is_device_reporting_) {
    LOG(ERROR) << "Timer triggered reporting failed. "
               << "Network_connected = " << network_connected_
               << ". Is device reporting = " << is_device_reporting_ << ".";
    return;
  }

  OnNetworkOnline();
}

void ReportController::OnNetworkOnline() {
  // Async wait for the system clock to synchronize on network connection.
  system_clock_sync_observation_ =
      SystemClockSyncObservation::WaitForSystemClockSync(
          SystemClockClient::Get(), kSystemClockSyncWaitTimeout,
          base::BindOnce(&ReportController::OnSystemClockSyncResult,
                         weak_factory_.GetWeakPtr()));
}

void ReportController::OnNetworkOffline() {
  // TODO: Evaluate if we should cancel callbacks here.
}

void ReportController::OnSystemClockSyncResult(bool system_clock_synchronized) {
  RecordIsSystemClockSynced(system_clock_synchronized);

  if (!system_clock_synchronized) {
    LOG(ERROR) << "System clock failed to be synchronized.";
    return;
  }

  StartReport();
}

void ReportController::StartReport() {
  DCHECK(local_state_);
  DCHECK(psm_client_manager_.get());

  // Get new adjusted timestamp from GMT to Pacific Time.
  active_ts_ = utils::ConvertGmtToPt(clock_);

  // Create instances of use cases and parameters.
  use_case_params_ = std::make_unique<device_metrics::UseCaseParameters>(
      active_ts_, chrome_device_params_, url_loader_factory_,
      high_entropy_seed_, local_state_, psm_client_manager_.get());
  one_day_impl_ =
      std::make_unique<device_metrics::OneDayImpl>(use_case_params_.get());
  twenty_eight_day_impl_ = std::make_unique<device_metrics::TwentyEightDayImpl>(
      use_case_params_.get());
  cohort_impl_ =
      std::make_unique<device_metrics::CohortImpl>(use_case_params_.get());
  observation_impl_ =
      std::make_unique<device_metrics::ObservationImpl>(use_case_params_.get());

  // Create callbacks to report use cases in a specific order, and also a
  // callback that updates the preserved file using the latest local state.
  // Note that the order of the use case callbacks is important.
  // Contact hirthanan@ or qianwan@ before making changes here.
  base::OnceClosure save_preserved_file_cb =
      CreateSavePreservedFileCallback(local_state_, weak_factory_.GetWeakPtr());
  base::OnceClosure report_observation_cb = CreateReportObservationCallback(
      observation_impl_->GetWeakPtr(), std::move(save_preserved_file_cb));
  base::OnceClosure report_cohort_cb = CreateReportCohortCallback(
      cohort_impl_->GetWeakPtr(), std::move(report_observation_cb));
  base::OnceClosure report_28da_cb = CreateReport28DaCallback(
      twenty_eight_day_impl_->GetWeakPtr(), std::move(report_cohort_cb));

  // Trigger sequential execution of callbacks by running first use case.
  is_device_reporting_ = true;

  one_day_impl_->Run(std::move(report_28da_cb));
}

}  // namespace ash::report
