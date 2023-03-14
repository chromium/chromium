// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_

#include <memory>
#include <queue>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/device_activity/churn_active_status.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "url/gurl.h"

class PrefService;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class NetworkState;
class NetworkStateHandler;
class SystemClockSyncObservation;

namespace device_activity {

// Forward declaration from device_active_use_case.h
class DeviceActiveUseCase;

// Observes the network for connected state to determine whether the device
// is active in a given window.
// State Transition flow:
// kIdle -> kCheckingMembershipOprf -> kCheckingMembershipQuery
// -> kIdle or (kCheckingIn -> kIdle)
//
// TODO(https://crbug.com/1302175): Move methods passing DeviceActiveUseCase* to
// methods of DeviceActiveUseCase class.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    DeviceActivityClient : public NetworkStateHandlerObserver {
 public:
  // Tracks the state the client is in, given the use case (i.e DAILY).
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class State {
    kUnknown = 0,  // Default value, typically we should never be in this state.
    kIdle = 1,     // Wait on network connection OR |report_timer_| to trigger.
    kCheckingMembershipOprf = 2,   // Phase 1 of the |CheckMembership| request.
    kCheckingMembershipQuery = 3,  // Phase 2 of the |CheckMembership| request.
    kCheckingIn = 4,               // |CheckIn| PSM device active request.
    kHealthCheck = 5,              // Query to perform server health check.
    kMaxValue = kHealthCheck,
  };

  // Categorize PSM response codes which will be used when bucketing UMA
  // histograms.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PsmResponse {
    kUnknown = 0,  // Uncategorized response type returned.
    kSuccess = 1,  // Successfully completed PSM request.
    kError = 2,    // Error completing PSM request.
    kTimeout = 3,  // Timed out while completing PSM request.
    kMaxValue = kTimeout,
  };

  // Categorize the preserved file state which will be used when bucketing UMA
  // histograms.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PreservedFileState {
    kUnknown = 0,                // Uncategorized state.
    kReadOkLocalStateEmpty = 1,  // Preserved file read successfully and local
                                 // state empty.
    kReadOkLocalStateSet = 2,    // Preserved file read successfully but local
                                 // state is already set.
    kReadFail = 3,  // Preserved file read failed and local state can either be
                    // empty or set.
    kMaxValue = kReadFail,
  };

  // Categorize device activity methods which will be used when bucketing UMA
  // histograms by number of calls to each method.
  // Enum listed keys map to methods in |DeviceActivityController| and
  // |DeviceActivityClient|.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeviceActivityMethod {
    kUnknown = 0,
    kDeviceActivityControllerConstructor = 1,
    kDeviceActivityControllerDestructor = 2,
    kDeviceActivityControllerStart = 3,
    kDeviceActivityControllerOnPsmDeviceActiveSecretFetched = 4,
    kDeviceActivityControllerOnMachineStatisticsLoaded = 5,
    kDeviceActivityClientConstructor = 6,
    kDeviceActivityClientDestructor = 7,
    kDeviceActivityClientOnNetworkOnline = 8,
    kDeviceActivityClientOnNetworkOffline = 9,
    kDeviceActivityClientReportUseCases = 10,
    kDeviceActivityClientCancelUseCases = 11,
    kDeviceActivityClientTransitionOutOfIdle = 12,
    kDeviceActivityClientTransitionToHealthCheck = 13,
    kDeviceActivityClientOnHealthCheckDone = 14,
    kDeviceActivityClientTransitionToCheckMembershipOprf = 15,
    kDeviceActivityClientOnCheckMembershipOprfDone = 16,
    kDeviceActivityClientTransitionToCheckMembershipQuery = 17,
    kDeviceActivityClientOnCheckMembershipQueryDone = 18,
    kDeviceActivityClientTransitionToCheckIn = 19,
    kDeviceActivityClientOnCheckInDone = 20,
    kDeviceActivityClientTransitionToIdle = 21,
    kDeviceActivityClientOnSystemClockSyncResult = 22,
    kDeviceActivityClientReportingTriggeredByTimer = 23,
    kDeviceActivityClientSaveLastPingDatesStatus = 24,
    kDeviceActivityClientOnSaveLastPingDatesStatusComplete = 25,
    kDeviceActivityClientGetLastPingDatesStatus = 26,
    kDeviceActivityClientOnGetLastPingDatesStatusFetched = 27,
    kMaxValue = kDeviceActivityClientOnGetLastPingDatesStatusFetched,
  };

  // Records UMA histogram for different failed cases before set the last
  // ping time to local_state_ after checking membership has response.
  enum class CheckMembershipResponseCases {
    kUnknown = 0,
    kCreateOprfRequestFailed = 1,
    kOprfResponseBodyFailed = 2,
    kNotHasRlweOprfResponse = 3,
    kCreateQueryRequestFailed = 4,
    kQueryResponseBodyFailed = 5,
    kNotHasRlweQueryResponse = 6,
    kProcessQueryResponseFailed = 7,
    kMembershipResponsesSizeIsNotOne = 8,
    kIsNotPsmIdMember = 9,
    kSuccessfullySetLocalState = 10,
    kMaxValue = kSuccessfullySetLocalState,
  };

  // Records UMA histogram for number of times various methods are called in
  // device_activity/.
  static void RecordDeviceActivityMethodCalled(
      DeviceActivityMethod method_name);

  // Fires device active pings while the device network is connected.
  DeviceActivityClient(
      ChurnActiveStatus* churn_active_status_ptr,
      PrefService* local_state,
      NetworkStateHandler* handler,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<base::RepeatingTimer> report_timer,
      const std::string& fresnel_server_url,
      const std::string& api_key,
      base::Time chrome_first_run_time,
      std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases);
  DeviceActivityClient(const DeviceActivityClient&) = delete;
  DeviceActivityClient& operator=(const DeviceActivityClient&) = delete;
  ~DeviceActivityClient() override;

  // Returns pointer to |report_timer_|.
  base::RepeatingTimer* GetReportTimer();

  // NetworkStateHandlerObserver overridden method.
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  State GetState() const;

  std::vector<DeviceActiveUseCase*> GetUseCases() const;

  DeviceActiveUseCase* GetUseCasePtr(
      private_membership::rlwe::RlweUseCase psm_use_case) const;

  // Update the local state values based after cohort check in updates the
  // active status value.
  // Specifically, the churn observation local state fields are relative to
  // the previous active status value:
  // 1. kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0
  // 2. kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1
  // 3. kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2
  void UpdateChurnLocalStateAfterCheckIn(DeviceActiveUseCase* current_use_case);

  // Read the cohort active status int32 and
  // period status booleans that indicate which observation periods
  // were already reported.
  //
  // Note the observation periods are relative to the current month stored
  // in the active status int32.
  // Method must also update the observation last ping timestamp in local state.
  void ReadChurnPreservedFile(const private_computing::ActiveStatus& status);

  // Generate the proto with the latest last ping date values.
  private_computing::SaveStatusRequest GetSaveStatusRequest();

  // Write the last ping dates for all use cases to preserved files via
  // the private_computingd dbus daemon.
  void SaveLastPingDatesStatus();

  // After the dbus call is complete, return response via this method.
  void OnSaveLastPingDatesStatusComplete(
      private_computing::SaveStatusResponse response);

  // Read the last ping dates status for all use cases from preserved files
  // via the private_comutingd dbus daemon.
  void GetLastPingDatesStatus();

  // After the dbus call is complete, return response via this method.
  void OnGetLastPingDatesStatusFetched(
      private_computing::GetStatusResponse response);

 private:
  // |report_timer_| triggers method to retry reporting device actives if
  // necessary.
  void ReportingTriggeredByTimer();

  // Handles device network connecting successfully.
  void OnNetworkOnline();

  // Called when the system clock has been synchronized or a timeout has been
  // reached while waiting for the system clock sync.
  // Report use cases if the system clock sync was successful.
  void OnSystemClockSyncResult(bool system_clock_synchronized);

  // Handle device network disconnecting successfully.
  void OnNetworkOffline();

  // Return Fresnel server network request endpoints determined by the |state_|.
  GURL GetFresnelURL() const;

  // Called when device network comes online or |report_timer_| executing.
  // Reports each use case in a sequenced order.
  void ReportUseCases();

  // Called when device network goes offline.
  // Since the network connection is severed, any pending network requests will
  // be cleaned up.
  // After calling this method: |state_| set to |kIdle|.
  void CancelUseCases();

  // Callback from |ReportUseCases()| handling whether a use case needs
  // to be reported for the time window.
  void TransitionOutOfIdle(DeviceActiveUseCase* current_use_case);

  // Send Health Check network request and update |state_|.
  // Before calling this method: |state_| is expected to be |kIdle|.
  // After calling this method: |state_| set to |kHealthCheck|.
  void TransitionToHealthCheck();

  // Callback from asynchronous method |TransitionToHealthCheck|.
  void OnHealthCheckDone(std::unique_ptr<std::string> response_body);

  // Send Oprf network request and update |state_|.
  // Before calling this method: |state_| is expected to be |kIdle|.
  // After calling this method:  |state_| set to |kCheckingMembershipOprf|.
  void TransitionToCheckMembershipOprf(DeviceActiveUseCase* current_use_case);

  // Callback from asynchronous method |TransitionToCheckMembershipOprf|.
  void OnCheckMembershipOprfDone(DeviceActiveUseCase* current_use_case,
                                 std::unique_ptr<std::string> response_body);

  // Send Query network request and update |state_|.
  // Before calling this method: |state_| is expected to be
  // |kCheckingMembershipOprf|.
  // After calling this method:  |state_| set to |kCheckingMembershipQuery|.
  void TransitionToCheckMembershipQuery(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response,
      DeviceActiveUseCase* current_use_case);

  // Callback from asynchronous method |TransitionToCheckMembershipQuery|.
  // Check in PSM id based on |response_body| from CheckMembershipQuery.
  void OnCheckMembershipQueryDone(DeviceActiveUseCase* current_use_case,
                                  std::unique_ptr<std::string> response_body);

  // Send Import network request and update |state_|.
  // Before calling this method: |state_| is expected to be either
  // |kCheckingMembershipQuery| or |kIdle|.
  // After calling this method:  |state_| set to |kCheckingIn|.
  void TransitionToCheckIn(DeviceActiveUseCase* current_use_case);

  // Callback from asynchronous method |TransitionToCheckIn|.
  void OnCheckInDone(DeviceActiveUseCase* current_use_case,
                     std::unique_ptr<std::string> response_body);

  // Updates |state_| to |kIdle| and resets state based member variables.
  void TransitionToIdle(DeviceActiveUseCase* current_use_case);

  // Tracks the current state of the DeviceActivityClient.
  State state_ = State::kIdle;

  // Used by the client to recover and maintain an updated value of churn active
  // status for the device active churn use cases.
  // |churn_active_status_ptr_| outlives the lifetime of this class and is owned
  // by the DeviceActivityController class.
  ChurnActiveStatus* const churn_active_status_ptr_;

  // Used by client to store and retrieve values stored in local state prefs.
  // |local_state_| outlives the lifetime of this class and interacts with the
  // churn active status, and churn observation period statuses.
  PrefService* const local_state_;

  // Keep track of whether the device is connected to the network.
  bool network_connected_ = false;

  // Time the device last transitioned out of idle state.
  base::Time last_transition_out_of_idle_time_;

  // Generated when entering new |state_| and reset when leaving |state_|.
  // This field is only used to determine total state duration, which is
  // reported to UMA via. histograms.
  base::ElapsedTimer state_timer_;

  // Tracks the visible networks and their properties.
  // |network_state_handler_| outlives the lifetime of this class.
  // |ChromeBrowserMainPartsAsh| initializes the network_state object as
  // part of the |dbus_services_|, before |DeviceActivityClient| is initialized.
  // Similarly, |DeviceActivityClient| is destructed before |dbus_services_|.
  NetworkStateHandler* const network_state_handler_;

  // Shared |url_loader_| object used to handle ongoing network requests.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The URLLoaderFactory we use to issue network requests.
  // |url_loader_factory_| outlives |url_loader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Tries reporting device actives every |kTimeToRepeat| from when this class
  // is initialized. Time of class initialization depends on when the device is
  // turned on (when |ChromeBrowserMainPartsAsh::PostBrowserStart| is run).
  std::unique_ptr<base::RepeatingTimer> report_timer_;

  // Base Fresnel server URL is set by |DeviceActivityClient| constructor.
  const std::string fresnel_base_url_;

  // API key used to authenticate with the Fresnel server. This key is read from
  // the chrome-internal repository and is not publicly exposed in Chromium.
  const std::string api_key_;

  // The chrome first run sentinel creation time.
  base::Time chrome_first_run_time_;

  // Vector of supported use cases containing the methods and metadata required
  // to counting device actives.
  std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases_;

  // Contains the use cases to report active for.
  // The front of the queue represents the use case trying to be reported.
  // |ReportUseCases| initializes this field using the |use_cases_|.
  // |TransitionToIdle| pops from this field to report each pending use case.
  std::queue<DeviceActiveUseCase*> pending_use_cases_;

  // Used to wait until the system clock to be synchronized.
  std::unique_ptr<SystemClockSyncObservation> system_clock_sync_observation_;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<DeviceActivityClient> weak_factory_{this};
};

}  // namespace device_activity
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
