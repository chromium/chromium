// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

// Schedules CryptAuth Enrollment and DeviceSync requests, alerting the
// respective Enrollment or DeviceSync delegate when a request is due via
// EnrollmentDelegate::OnEnrollmentRequest() or
// DeviceSyncDelegate::OnDeviceSyncRequested(). Enrollment and DeviceSync
// scheduling begins on StartEnrollmentScheduling() and
// StartDeviceSyncScheduling(), respectively.
//
// The scheduler client can bypass the current schedule and trigger an
// Enrollment/DeviceSync request via RequestEnrollment()/RequestDeviceSync().
// When an Enrollment/DeviceSync attempt has completed--successfully or not--the
// client should invoke HandleEnrollmentResult()/HandleDeviceSyncResult() so the
// scheduler can process the attempt outcomes and schedule future attempts if
// necessary.
class CryptAuthScheduler {
 public:
  class EnrollmentDelegate {
   public:
    EnrollmentDelegate() = default;
    virtual ~EnrollmentDelegate() = default;

    // Called to alert the delegate that an Enrollment attempt has been
    // requested by the scheduler.
    //   |client_metadata|: Contains the retry count, invocation reason, and
    //       possible session ID of the Enrollment request.
    //   |client_directive_policy_reference|: Identifies the CryptAuth policy
    //       associated with the ClientDirective parameters used to schedule
    //       this Enrollment attempt. If no ClientDirective was used by the
    //       scheduler, std::nullopt is passed.
    virtual void OnEnrollmentRequested(
        const cryptauthv2::ClientMetadata& client_metadata,
        const std::optional<cryptauthv2::PolicyReference>&
            client_directive_policy_reference) = 0;
  };

  class DeviceSyncDelegate {
   public:
    DeviceSyncDelegate() = default;
    virtual ~DeviceSyncDelegate() = default;

    // Called to alert the delegate that a DeviceSync attempt has been requested
    // by the scheduler.
    //   |client_metadata|: Contains the retry count, invocation reason, and
    //       possible session ID of the DeviceSync request.
    virtual void OnDeviceSyncRequested(
        const cryptauthv2::ClientMetadata& client_metadata) = 0;
  };

  CryptAuthScheduler(const CryptAuthScheduler&) = delete;
  CryptAuthScheduler& operator=(const CryptAuthScheduler&) = delete;

  virtual ~CryptAuthScheduler();

  // Note: These should only be called once.
  void StartEnrollmentScheduling(
      const base::WeakPtr<EnrollmentDelegate>& enrollment_delegate);
  void StartDeviceSyncScheduling(
      const base::WeakPtr<DeviceSyncDelegate>& device_sync_delegate);

  bool HasEnrollmentSchedulingStarted();
  bool HasDeviceSyncSchedulingStarted();

  // Requests an Enrollment/DeviceSync with the desired |invocation_reason| and,
  // if relevant, the |session_id| of the GCM message that requested an
  // Enrollment/DeviceSync.
  virtual void RequestEnrollment(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) = 0;
  virtual void RequestDeviceSync(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) = 0;

  // Processes the result of the previous Enrollment/DeviceSync attempt.
  virtual void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) = 0;
  virtual void HandleDeviceSyncResult(
      const CryptAuthDeviceSyncResult& device_sync_result) = 0;

  // Returns the time of the last known successful Enrollment/DeviceSync. If no
  // successful Enrollment/DeviceSync has occurred, std::nullopt is returned.
  virtual std::optional<base::Time> GetLastSuccessfulEnrollmentTime() const = 0;
  virtual std::optional<base::Time> GetLastSuccessfulDeviceSyncTime() const = 0;

  // Returns the scheduler's time period between a successful Enrollment and its
  // next Enrollment request.
  virtual base::TimeDelta GetRefreshPeriod() const = 0;

  // Returns the time until the next scheduled Enrollment/DeviceSync request.
  // Returns null if there is no request scheduled.
  virtual std::optional<base::TimeDelta> GetTimeToNextEnrollmentRequest()
      const = 0;
  virtual std::optional<base::TimeDelta> GetTimeToNextDeviceSyncRequest()
      const = 0;

  // Returns true after the Enrollment/DeviceSync delegate has been alerted of a
  // request but before the delegate has returned the result to the scheduler.
  // In other words, between
  // {Enrollment,DeviceSync}Delegate::On{Enrollment,DeviceSync}Requested() and
  // Handle{Enrollment,DeviceSync}Result().
  virtual bool IsWaitingForEnrollmentResult() const = 0;
  virtual bool IsWaitingForDeviceSyncResult() const = 0;

  // The number of times the current Enrollment/DeviceSync request has failed.
  // Once the Enrollment/DeviceSync request succeeds or a fresh request is
  // made--for example, via a forced Enrollment/DeviceSync--this counter is
  // reset.
  virtual size_t GetNumConsecutiveEnrollmentFailures() const = 0;
  virtual size_t GetNumConsecutiveDeviceSyncFailures() const = 0;

 protected:
  CryptAuthScheduler();

  virtual void OnEnrollmentSchedulingStarted();
  virtual void OnDeviceSyncSchedulingStarted();

  // Alerts the Enrollment/DeviceSync delegate that an Enrollment/DeviceSync has
  // been requested.
  void NotifyEnrollmentRequested(
      const cryptauthv2::ClientMetadata& client_metadata,
      const std::optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) const;
  void NotifyDeviceSyncRequested(
      const cryptauthv2::ClientMetadata& client_metadata) const;

 private:
  base::WeakPtr<EnrollmentDelegate> enrollment_delegate_;
  base::WeakPtr<DeviceSyncDelegate> device_sync_delegate_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_H_
