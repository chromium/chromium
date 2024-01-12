// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

class NetworkStateHandler;

namespace device_sync {

// CryptAuthScheduler implementation which stores scheduling metadata
// persistently so that the Enrollment and DeviceSync schedules are saved across
// device reboots. Requests made before scheduling has started, while an
// Enrollment/DeviceSync attempt is in progress, or while offline are cached and
// rescheduled as soon as possible.
//
// If an Enrollment has never successfully completed, an initialization request
// is made immediately on startup. After a successful Enrollment, periodic
// Enrollment requests are made at time intervals provided by the CryptAuth
// server in the ClientDirective proto. Note that there is no periodic
// scheduling for DeviceSync, per se; however, an initialization request will be
// made.
//
// ClientDirectives received from CryptAuth may contain an InvokeNext field
// specifying that an Enrollment and/or DeviceSync request should be made
// after the current Enrollment or DeviceSync attempt finishes successfully.
// These subsequent requests are handled internally by the scheduler.
//
// All failed Enrollment/DeviceSync attempts will be retried at time intervals
// specified in the ClientDirective proto.
class CryptAuthSchedulerImpl : public CryptAuthScheduler,
                               public NetworkStateHandlerObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthScheduler> Create(
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler =
            NetworkHandler::Get()->network_state_handler(),
        base::Clock* clock = base::DefaultClock::GetInstance(),
        std::unique_ptr<base::OneShotTimer> enrollment_timer =
            std::make_unique<base::OneShotTimer>(),
        std::unique_ptr<base::OneShotTimer> device_sync_timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthScheduler> CreateInstance(
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler,
        base::Clock* clock,
        std::unique_ptr<base::OneShotTimer> enrollment_timer,
        std::unique_ptr<base::OneShotTimer> device_sync_timer) = 0;

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  CryptAuthSchedulerImpl(const CryptAuthSchedulerImpl&) = delete;
  CryptAuthSchedulerImpl& operator=(const CryptAuthSchedulerImpl&) = delete;

  ~CryptAuthSchedulerImpl() override;

 private:
  enum class RequestType { kEnrollment, kDeviceSync };

  static std::string GetLastAttemptTimePrefName(RequestType request_type);
  static std::string GetLastSuccessTimePrefName(RequestType request_type);
  static std::string GetPendingRequestPrefName(RequestType request_type);

  CryptAuthSchedulerImpl(PrefService* pref_service,
                         NetworkStateHandler* network_state_handler,
                         base::Clock* clock,
                         std::unique_ptr<base::OneShotTimer> enrollment_timer,
                         std::unique_ptr<base::OneShotTimer> device_sync_timer);

  // CryptAuthScheduler:
  void OnEnrollmentSchedulingStarted() override;
  void OnDeviceSyncSchedulingStarted() override;
  void RequestEnrollment(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) override;
  void RequestDeviceSync(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  void HandleDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) override;
  std::optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  std::optional<base::Time> GetLastSuccessfulDeviceSyncTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  std::optional<base::TimeDelta> GetTimeToNextEnrollmentRequest()
      const override;
  std::optional<base::TimeDelta> GetTimeToNextDeviceSyncRequest()
      const override;
  bool IsWaitingForEnrollmentResult() const override;
  bool IsWaitingForDeviceSyncResult() const override;
  size_t GetNumConsecutiveEnrollmentFailures() const override;
  size_t GetNumConsecutiveDeviceSyncFailures() const override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // Shared logic between Enrollment and DeviceSync CryptAuthScheduler methods.
  void OnSchedulingStarted(RequestType request_type);
  void MakeRequest(
      RequestType request_type,
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id);
  void HandleResult(
      RequestType request_type,
      bool success,
      const std::optional<cryptauthv2::ClientDirective>& new_client_directive);
  void HandleInvokeNext(const ::google::protobuf::RepeatedPtrField<
                            cryptauthv2::InvokeNext>& invoke_next_list,
                        const std::string& session_id);
  std::optional<base::Time> GetLastSuccessTime(RequestType request_type) const;
  std::optional<base::TimeDelta> GetTimeToNextRequest(
      RequestType request_type) const;
  bool IsWaitingForResult(RequestType request_type) const;
  size_t GetNumConsecutiveFailures(RequestType request_type) const;

  // Returns true if online.
  bool DoesMachineHaveNetworkConnectivity() const;

  // Sets the pending requests from the persisted pref values.
  void InitializePendingRequest(RequestType request_type);

  // Starts a new timer that will fire when an Enrollment/DeviceSync is ready to
  // be attempted. Early returns if an Enrollment/DeviceSync is already in
  // progress, if scheduling hasn't started, or if there is no request to be
  // made. The request is persisted to prefs if another Enrollment/DeviceSync
  // attempt isn't already in progress.
  void ScheduleNextRequest(RequestType request_type);

  // Notifies the Enrollment/DeviceSync delegate that an Enrollment/DeviceSync
  // attempt should be made. Early returns if not online; the attempt is
  // rescheduled when connectivity is restored.
  void OnTimerFired(RequestType request_type);

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<base::Clock> clock_ = nullptr;
  cryptauthv2::ClientDirective client_directive_;
  base::flat_map<RequestType, std::unique_ptr<base::OneShotTimer>>
      request_timers_;
  base::flat_map<RequestType, std::optional<cryptauthv2::ClientMetadata>>
      pending_requests_{{RequestType::kEnrollment, std::nullopt},
                        {RequestType::kDeviceSync, std::nullopt}};

  // Values only non-null while an attempt is in progress, in other words,
  // between Notify*Requested() and Handle*Result().
  base::flat_map<RequestType, std::optional<cryptauthv2::ClientMetadata>>
      current_requests_{{RequestType::kEnrollment, std::nullopt},
                        {RequestType::kDeviceSync, std::nullopt}};
};

}  // namespace device_sync
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_
