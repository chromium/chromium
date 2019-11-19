// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

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
// scheduling for DeviceSync, per se.
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
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthScheduler> BuildInstance(
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler =
            NetworkHandler::Get()->network_state_handler(),
        base::Clock* clock = base::DefaultClock::GetInstance(),
        std::unique_ptr<base::OneShotTimer> enrollment_timer =
            std::make_unique<base::OneShotTimer>(),
        std::unique_ptr<base::OneShotTimer> device_sync_timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

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
      const base::Optional<std::string>& session_id) override;
  void RequestDeviceSync(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const base::Optional<std::string>& session_id) override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  void HandleDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) override;
  base::Optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  base::Optional<base::Time> GetLastSuccessfulDeviceSyncTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  base::Optional<base::TimeDelta> GetTimeToNextEnrollmentRequest()
      const override;
  base::Optional<base::TimeDelta> GetTimeToNextDeviceSyncRequest()
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
      const base::Optional<std::string>& session_id);
  void HandleResult(
      RequestType request_type,
      bool success,
      const base::Optional<cryptauthv2::ClientDirective>& new_client_directive);
  void HandleInvokeNext(const ::google::protobuf::RepeatedPtrField<
                            cryptauthv2::InvokeNext>& invoke_next_list,
                        const std::string& session_id);
  base::Optional<base::Time> GetLastSuccessTime(RequestType request_type) const;
  base::Optional<base::TimeDelta> GetTimeToNextRequest(
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

  PrefService* pref_service_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  base::Clock* clock_ = nullptr;
  cryptauthv2::ClientDirective client_directive_;
  base::flat_map<RequestType, std::unique_ptr<base::OneShotTimer>>
      request_timers_;
  base::flat_map<RequestType, base::Optional<cryptauthv2::ClientMetadata>>
      pending_requests_{{RequestType::kEnrollment, base::nullopt},
                        {RequestType::kDeviceSync, base::nullopt}};

  // Values only non-null while an attempt is in progress, in other words,
  // between Notify*Requested() and Handle*Result().
  base::flat_map<RequestType, base::Optional<cryptauthv2::ClientMetadata>>
      current_requests_{{RequestType::kEnrollment, base::nullopt},
                        {RequestType::kDeviceSync, base::nullopt}};

  DISALLOW_COPY_AND_ASSIGN(CryptAuthSchedulerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_
