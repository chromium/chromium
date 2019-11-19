// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_scheduler.h"

#include "base/logging.h"

namespace chromeos {

namespace device_sync {

constexpr base::TimeDelta FakeCryptAuthScheduler::kDefaultRefreshPeriod;
constexpr base::TimeDelta
    FakeCryptAuthScheduler::kDefaultTimeToNextEnrollmentRequest;

FakeCryptAuthScheduler::FakeCryptAuthScheduler() = default;

FakeCryptAuthScheduler::~FakeCryptAuthScheduler() = default;

void FakeCryptAuthScheduler::RequestEnrollment(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const base::Optional<std::string>& session_id) {
  DCHECK(HasEnrollmentSchedulingStarted());
  is_waiting_for_enrollment_result_ = true;

  cryptauthv2::ClientMetadata client_metadata;
  client_metadata.set_retry_count(num_consecutive_enrollment_failures_);
  client_metadata.set_invocation_reason(invocation_reason);
  if (session_id)
    client_metadata.set_session_id(*session_id);

  NotifyEnrollmentRequested(client_metadata,
                            client_directive_policy_reference_);
}

void FakeCryptAuthScheduler::RequestDeviceSync(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const base::Optional<std::string>& session_id) {
  DCHECK(HasDeviceSyncSchedulingStarted());
  is_waiting_for_device_sync_result_ = true;

  cryptauthv2::ClientMetadata client_metadata;
  client_metadata.set_retry_count(num_consecutive_device_sync_failures_);
  client_metadata.set_invocation_reason(invocation_reason);
  if (session_id)
    client_metadata.set_session_id(*session_id);

  NotifyDeviceSyncRequested(client_metadata);
}

void FakeCryptAuthScheduler::HandleEnrollmentResult(
    const CryptAuthEnrollmentResult& enrollment_result) {
  DCHECK(is_waiting_for_enrollment_result_);
  handled_enrollment_results_.push_back(enrollment_result);
  is_waiting_for_enrollment_result_ = false;
}

void FakeCryptAuthScheduler::HandleDeviceSyncResult(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  DCHECK(is_waiting_for_device_sync_result_);
  handled_device_sync_results_.push_back(device_sync_result);
  is_waiting_for_device_sync_result_ = false;
}

base::Optional<base::Time>
FakeCryptAuthScheduler::GetLastSuccessfulEnrollmentTime() const {
  return last_successful_enrollment_time_;
}

base::Optional<base::Time>
FakeCryptAuthScheduler::GetLastSuccessfulDeviceSyncTime() const {
  return last_successful_device_sync_time_;
}

base::TimeDelta FakeCryptAuthScheduler::GetRefreshPeriod() const {
  return refresh_period_;
}

base::Optional<base::TimeDelta>
FakeCryptAuthScheduler::GetTimeToNextEnrollmentRequest() const {
  return time_to_next_enrollment_request_;
}

base::Optional<base::TimeDelta>
FakeCryptAuthScheduler::GetTimeToNextDeviceSyncRequest() const {
  return time_to_next_device_sync_request_;
}

bool FakeCryptAuthScheduler::IsWaitingForEnrollmentResult() const {
  return is_waiting_for_enrollment_result_;
}

bool FakeCryptAuthScheduler::IsWaitingForDeviceSyncResult() const {
  return is_waiting_for_device_sync_result_;
}

size_t FakeCryptAuthScheduler::GetNumConsecutiveEnrollmentFailures() const {
  return num_consecutive_enrollment_failures_;
}

size_t FakeCryptAuthScheduler::GetNumConsecutiveDeviceSyncFailures() const {
  return num_consecutive_device_sync_failures_;
}

FakeCryptAuthSchedulerEnrollmentDelegate::
    FakeCryptAuthSchedulerEnrollmentDelegate() {}

FakeCryptAuthSchedulerEnrollmentDelegate::
    ~FakeCryptAuthSchedulerEnrollmentDelegate() = default;

base::WeakPtr<FakeCryptAuthSchedulerEnrollmentDelegate>
FakeCryptAuthSchedulerEnrollmentDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeCryptAuthSchedulerEnrollmentDelegate::OnEnrollmentRequested(
    const cryptauthv2::ClientMetadata& client_metadata,
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  client_metadata_from_enrollment_requests_.push_back(client_metadata);
  policy_references_from_enrollment_requests_.push_back(
      client_directive_policy_reference);
}

FakeCryptAuthSchedulerDeviceSyncDelegate::
    FakeCryptAuthSchedulerDeviceSyncDelegate() {}

FakeCryptAuthSchedulerDeviceSyncDelegate::
    ~FakeCryptAuthSchedulerDeviceSyncDelegate() = default;

base::WeakPtr<FakeCryptAuthSchedulerDeviceSyncDelegate>
FakeCryptAuthSchedulerDeviceSyncDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeCryptAuthSchedulerDeviceSyncDelegate::OnDeviceSyncRequested(
    const cryptauthv2::ClientMetadata& client_metadata) {
  client_metadata_from_device_sync_requests_.push_back(client_metadata);
}

}  // namespace device_sync

}  // namespace chromeos
