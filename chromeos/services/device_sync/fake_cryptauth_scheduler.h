// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_SCHEDULER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_SCHEDULER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

// Fake CryptAuthScheduler implementation.
class FakeCryptAuthScheduler : public CryptAuthScheduler {
 public:
  static constexpr base::TimeDelta kDefaultRefreshPeriod =
      base::TimeDelta::FromDays(30);
  static constexpr base::TimeDelta kDefaultTimeToNextEnrollmentRequest =
      base::TimeDelta::FromHours(12);

  FakeCryptAuthScheduler();
  ~FakeCryptAuthScheduler() override;

  const std::vector<CryptAuthEnrollmentResult>& handled_enrollment_results()
      const {
    return handled_enrollment_results_;
  }

  const std::vector<CryptAuthDeviceSyncResult>& handled_device_sync_results()
      const {
    return handled_device_sync_results_;
  }

  void set_client_directive_policy_reference(
      const base::Optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) {
    client_directive_policy_reference_ = client_directive_policy_reference;
  }

  void set_last_successful_enrollment_time(
      base::Optional<base::Time> last_successful_enrollment_time) {
    last_successful_enrollment_time_ = last_successful_enrollment_time;
  }

  void set_last_successful_device_sync_time(
      base::Optional<base::Time> last_successful_device_sync_time) {
    last_successful_device_sync_time_ = last_successful_device_sync_time;
  }

  void set_refresh_period(base::TimeDelta refresh_period) {
    refresh_period_ = refresh_period;
  }

  void set_time_to_next_enrollment_request(
      base::Optional<base::TimeDelta> time_to_next_enrollment_request) {
    time_to_next_enrollment_request_ = time_to_next_enrollment_request;
  }

  void set_time_to_next_device_sync_request(
      base::Optional<base::TimeDelta> time_to_next_device_sync_request) {
    time_to_next_device_sync_request_ = time_to_next_device_sync_request;
  }

  void set_num_consecutive_enrollment_failures(
      size_t num_consecutive_enrollment_failures) {
    num_consecutive_enrollment_failures_ = num_consecutive_enrollment_failures;
  }

  void set_num_consecutive_device_sync_failures(
      size_t num_consecutive_device_sync_failures) {
    num_consecutive_device_sync_failures_ =
        num_consecutive_device_sync_failures;
  }

  // CryptAuthScheduler:
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

 private:
  std::vector<CryptAuthEnrollmentResult> handled_enrollment_results_;
  std::vector<CryptAuthDeviceSyncResult> handled_device_sync_results_;
  base::Optional<cryptauthv2::PolicyReference>
      client_directive_policy_reference_;
  base::Optional<base::Time> last_successful_enrollment_time_;
  base::Optional<base::Time> last_successful_device_sync_time_;
  base::TimeDelta refresh_period_ = kDefaultRefreshPeriod;
  base::Optional<base::TimeDelta> time_to_next_enrollment_request_ =
      kDefaultTimeToNextEnrollmentRequest;
  base::Optional<base::TimeDelta> time_to_next_device_sync_request_;
  size_t num_consecutive_enrollment_failures_ = 0u;
  size_t num_consecutive_device_sync_failures_ = 0u;
  bool is_waiting_for_enrollment_result_ = false;
  bool is_waiting_for_device_sync_result_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthScheduler);
};

// Fake CryptAuthScheduler::EnrollmentDelegate implementation.
class FakeCryptAuthSchedulerEnrollmentDelegate
    : public CryptAuthScheduler::EnrollmentDelegate {
 public:
  FakeCryptAuthSchedulerEnrollmentDelegate();
  ~FakeCryptAuthSchedulerEnrollmentDelegate() override;

  base::WeakPtr<FakeCryptAuthSchedulerEnrollmentDelegate> GetWeakPtr();

  const std::vector<cryptauthv2::ClientMetadata>&
  client_metadata_from_enrollment_requests() const {
    return client_metadata_from_enrollment_requests_;
  }

  const std::vector<base::Optional<cryptauthv2::PolicyReference>>&
  policy_references_from_enrollment_requests() const {
    return policy_references_from_enrollment_requests_;
  }

 private:
  // CryptAuthScheduler::EnrollmentDelegate:
  void OnEnrollmentRequested(const cryptauthv2::ClientMetadata& client_metadata,
                             const base::Optional<cryptauthv2::PolicyReference>&
                                 client_directive_policy_reference) override;

  std::vector<cryptauthv2::ClientMetadata>
      client_metadata_from_enrollment_requests_;
  std::vector<base::Optional<cryptauthv2::PolicyReference>>
      policy_references_from_enrollment_requests_;
  base::WeakPtrFactory<FakeCryptAuthSchedulerEnrollmentDelegate>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthSchedulerEnrollmentDelegate);
};

// Fake CryptAuthScheduler::DeviceSyncDelegate implementation.
class FakeCryptAuthSchedulerDeviceSyncDelegate
    : public CryptAuthScheduler::DeviceSyncDelegate {
 public:
  FakeCryptAuthSchedulerDeviceSyncDelegate();
  ~FakeCryptAuthSchedulerDeviceSyncDelegate() override;

  base::WeakPtr<FakeCryptAuthSchedulerDeviceSyncDelegate> GetWeakPtr();

  const std::vector<cryptauthv2::ClientMetadata>&
  client_metadata_from_device_sync_requests() const {
    return client_metadata_from_device_sync_requests_;
  }

 private:
  // CryptAuthScheduler::DeviceSyncDelegate:
  void OnDeviceSyncRequested(
      const cryptauthv2::ClientMetadata& client_metadata) override;

  std::vector<cryptauthv2::ClientMetadata>
      client_metadata_from_device_sync_requests_;
  base::WeakPtrFactory<FakeCryptAuthSchedulerDeviceSyncDelegate>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthSchedulerDeviceSyncDelegate);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_SCHEDULER_H_
