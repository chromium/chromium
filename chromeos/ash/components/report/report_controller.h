// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_REPORT_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_REPORT_CONTROLLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/report/device_metrics/use_case/psm_client_manager.h"
#include "chromeos/ash/components/report/device_metrics/use_case/use_case.h"
#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace base {
class Clock;
}  // namespace base
namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefRegistrySimple;
class PrefService;

namespace private_computing {
class GetStatusResponse;
class SaveStatusResponse;
}  // namespace private_computing

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash {

class NetworkState;
class NetworkStateHandler;
class SystemClockSyncObservation;

namespace system {
class StatisticsProvider;
}  // namespace system

namespace report {

namespace device_metrics {
class OneDayImpl;
class TwentyEightDayImpl;
class CohortImpl;
class ObservationImpl;

struct ChromeDeviceMetadataParameters;
}  // namespace device_metrics

// Observe the network in order to report metrics to Fresnel
// using private set membership.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) ReportController
    : public NetworkStateHandlerObserver {
 public:
  // Retrieves a singleton instance.
  static ReportController* Get();

  // Registers local state preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Determine start up delay before reporting should starts.
  static base::TimeDelta DetermineStartUpDelay(base::Time chrome_first_run_ts);

  // Determine market segment from the loaded ChromeOS device policies.
  static MarketSegment GetMarketSegment(
      policy::DeviceMode device_mode,
      policy::MarketSegment device_market_segment);

  ReportController(
      const device_metrics::ChromeDeviceMetadataParameters&
          chrome_device_params,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Time chrome_first_run_time,
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback,
      base::RepeatingCallback<policy::DeviceMode()> device_mode_callback,
      base::RepeatingCallback<policy::MarketSegment()> market_segment_callback,
      std::unique_ptr<device_metrics::PsmClientManager> psm_client_manager);
  ReportController(const ReportController&) = delete;
  ReportController& operator=(const ReportController&) = delete;
  ~ReportController() override;

  // NetworkStateHandlerObserver overridden method.
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // Handler after saving fresnel prefs values to the preserved file completed.
  void OnSaveLocalStateToPreservedFileComplete(
      private_computing::SaveStatusResponse response);

  // Testing
  bool IsDeviceReportingForTesting() const;

 private:
  // Grant friend access for comprehensive testing of private/protected members.
  friend class ReportControllerTestBase;
  friend class ReportControllerSimpleFlowTest;

  // Wrapper method for the PostTaskAndReplyWithResult, which is used to spawn
  // a worker thread to check oobe completed file time delta.
  void CheckOobeCompletedInWorker(
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback);

  // Retry method every kOobeReadFailedRetryDelay minute until confirming
  // 1 minute has passed since /home/chronos/.oobe_completed file was written.
  // Maximum retry count is kNumberOfRetriesBeforeFail.
  void OnOobeFileWritten(
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback,
      base::TimeDelta time_since_oobe_file_written);

  // Read the high entropy seed from VPD over DBus.
  // This device secret must be fetched correctly in order for the device
  // to generate pseudonymous identifiers.
  void OnPsmDeviceActiveSecretFetched(const std::string& high_entropy_seed);

  // Verify that the device machine statistics are set and loaded.
  // This is important in order to correctly generate various metadata being
  // transmitted in Fresnel import pings.
  void OnMachineStatisticsLoaded();

  // Preserved file is used to restore local state before attempting to report
  // metrics. Local state is lost After a simple powerwash occurs on the device.
  void OnPreservedFileReadComplete(
      private_computing::GetStatusResponse response);

  // Prerequisites before attempting to report metrics were completed
  // successfully. Set-up reporting based on timer and network connection.
  void OnReadyToReport();

  // Retry reporting metrics.
  void ReportingTriggeredByTimer();

  // Handles device network connecting successfully.
  void OnNetworkOnline();

  // Handles device network disconnecting.
  void OnNetworkOffline();

  // Called when the system clock has been synchronized or a timeout has been
  // reached while waiting for the system clock sync.
  // Report use cases if the system clock sync was successful.
  void OnSystemClockSyncResult(bool system_clock_synchronized);

  // If requirements are successfully met, start reporting flow in sequential
  // order per use case.
  void StartReport();

  // Chrome browser passed parameters that live throughout this class lifetime.
  // Market segment field is only assigned after oobe is completed and the
  // market segment is known.
  device_metrics::ChromeDeviceMetadataParameters chrome_device_params_;

  // Update relevant pref keys with preserved file data if missing.
  // Pref keys are found in //report/prefs/fresnel_pref_names.h
  // Pointer will exist throughout class lifetime.
  const raw_ptr<PrefService, ExperimentalAsh> local_state_;

  // Field is used to handle Fresnel requests/responses on a single sequence.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The chrome first run sentinel creation time.
  base::Time chrome_first_run_time_;

  // Store callbacks to retrieve the policy device mode and market segment.
  // Method should be called after oobe is successfully completed, and the
  // .oobe_completed file is written.
  // Callback will outlive this class, and is managed by ash-chrome.
  base::RepeatingCallback<policy::DeviceMode()> device_mode_callback_;
  base::RepeatingCallback<policy::MarketSegment()> market_segment_callback_;

  // Try report metrics every |kTimeToRepeat|.
  std::unique_ptr<base::RepeatingTimer> report_timer_;

  // Tracks the visible networks and their properties.
  // |network_state_handler_| outlives the lifetime of this class.
  const raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_;

  // Singleton lives throughout class lifetime.
  const raw_ptr<system::StatisticsProvider, ExperimentalAsh>
      statistics_provider_;

  // Number of retry attempts at reading the oobe completed file.
  int retry_oobe_completed_count_ = 0;

  // Timer used to retry reading oobe_completed file.
  std::unique_ptr<base::OneShotTimer> oobe_completed_timer_;

  // Set high entropy seed, which is passed to the use cases in order to
  // uniquely generate pseudonymous ids when importing.
  // Field is set after OnPsmDeviceActiveSecretFetched is called.
  std::string high_entropy_seed_;

  // Keep track of whether the device is connected to the network.
  bool network_connected_ = false;

  // Used to wait until the system clock to be synchronized.
  std::unique_ptr<SystemClockSyncObservation> system_clock_sync_observation_;

  // Used to get timestamp used in updating |active_ts_| when reporting.
  raw_ptr<base::Clock> clock_;

  // Time at which the device will report active.
  // Set every time the device comes online, or report timer triggers.
  base::Time active_ts_;

  // Set flag to true while the device is executing reporting flow.
  bool is_device_reporting_ = false;

  // Used to enable passing the real and fake PSM client to this class.
  std::unique_ptr<device_metrics::PsmClientManager> psm_client_manager_;

  // Store parameters used across the reporting use cases throughout reporting
  // object lifetime.
  std::unique_ptr<device_metrics::UseCaseParameters> use_case_params_;

  // Maintains object through lifetime of this instance.
  std::unique_ptr<device_metrics::OneDayImpl> one_day_impl_;

  // Maintains object through lifetime of this instance.
  std::unique_ptr<device_metrics::TwentyEightDayImpl> twenty_eight_day_impl_;

  // Maintains object through lifetime of this instance.
  std::unique_ptr<device_metrics::CohortImpl> cohort_impl_;

  // Maintains object through lifetime of this instance.
  std::unique_ptr<device_metrics::ObservationImpl> observation_impl_;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<ReportController> weak_factory_{this};
};

}  // namespace report

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_REPORT_CONTROLLER_H_
