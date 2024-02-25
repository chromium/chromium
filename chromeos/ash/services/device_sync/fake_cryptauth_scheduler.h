// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_SCHEDULER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_SCHEDULER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

// Fake CryptAuthScheduler implementation.
class FakeCryptAuthScheduler : public CryptAuthScheduler {
 public:
  static constexpr base::TimeDelta kDefaultRefreshPeriod = base::Days(30);
  static constexpr base::TimeDelta kDefaultTimeToNextEnrollmentRequest =
      base::Hours(12);

  FakeCryptAuthScheduler();

  FakeCryptAuthScheduler(const FakeCryptAuthScheduler&) = delete;
  FakeCryptAuthScheduler& operator=(const FakeCryptAuthScheduler&) = delete;

  ~FakeCryptAuthScheduler() override;

  size_t num_enrollment_requests() const { return num_enrollment_requests_; }
  size_t num_sync_requests() const { return num_sync_requests_; }

  const std::vector<CryptAuthEnrollmentResult>& handled_enrollment_results()
      const {
    return handled_enrollment_results_;
  }

  const std::vector<CryptAuthDeviceSyncResult>& handled_device_sync_results()
      const {
    return handled_device_sync_results_;
  }

  void set_client_directive_policy_reference(
      const std::optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) {
    client_directive_policy_reference_ = client_directive_policy_reference;
  }

  void set_last_successful_enrollment_time(
      std::optional<base::Time> last_successful_enrollment_time) {
    last_successful_enrollment_time_ = last_successful_enrollment_time;
  }

  void set_last_successful_device_sync_time(
      std::optional<base::Time> last_successful_device_sync_time) {
    last_successful_device_sync_time_ = last_successful_device_sync_time;
  }

  void set_refresh_period(base::TimeDelta refresh_period) {
    refresh_period_ = refresh_period;
  }

  void set_time_to_next_enrollment_request(
      std::optional<base::TimeDelta> time_to_next_enrollment_request) {
    time_to_next_enrollment_request_ = time_to_next_enrollment_request;
  }

  void set_time_to_next_device_sync_request(
      std::optional<base::TimeDelta> time_to_next_device_sync_request) {
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

 private:
  std::vector<CryptAuthEnrollmentResult> handled_enrollment_results_;
  std::vector<CryptAuthDeviceSyncResult> handled_device_sync_results_;
  std::optional<cryptauthv2::PolicyReference>
      client_directive_policy_reference_;
  std::optional<base::Time> last_successful_enrollment_time_;
  std::optional<base::Time> last_successful_device_sync_time_;
  base::TimeDelta refresh_period_ = kDefaultRefreshPeriod;
  std::optional<base::TimeDelta> time_to_next_enrollment_request_ =
      kDefaultTimeToNextEnrollmentRequest;
  std::optional<base::TimeDelta> time_to_next_device_sync_request_;
  size_t num_enrollment_requests_ = 0u;
  size_t num_sync_requests_ = 0u;
  size_t num_consecutive_enrollment_failures_ = 0u;
  size_t num_consecutive_device_sync_failures_ = 0u;
  bool is_waiting_for_enrollment_result_ = false;
  bool is_waiting_for_device_sync_result_ = false;
};

// Fake CryptAuthScheduler::EnrollmentDelegate implementation.
class FakeCryptAuthSchedulerEnrollmentDelegate
    : public CryptAuthScheduler::EnrollmentDelegate {
 public:
  FakeCryptAuthSchedulerEnrollmentDelegate();

  FakeCryptAuthSchedulerEnrollmentDelegate(
      const FakeCryptAuthSchedulerEnrollmentDelegate&) = delete;
  FakeCryptAuthSchedulerEnrollmentDelegate& operator=(
      const FakeCryptAuthSchedulerEnrollmentDelegate&) = delete;

  ~FakeCryptAuthSchedulerEnrollmentDelegate() override;

  base::WeakPtr<FakeCryptAuthSchedulerEnrollmentDelegate> GetWeakPtr();

  const std::vector<cryptauthv2::ClientMetadata>&
  client_metadata_from_enrollment_requests() const {
    return client_metadata_from_enrollment_requests_;
  }

  const std::vector<std::optional<cryptauthv2::PolicyReference>>&
  policy_references_from_enrollment_requests() const {
    return policy_references_from_enrollment_requests_;
  }

 private:
  // CryptAuthScheduler::EnrollmentDelegate:
  void OnEnrollmentRequested(const cryptauthv2::ClientMetadata& client_metadata,
                             const std::optional<cryptauthv2::PolicyReference>&
                                 client_directive_policy_reference) override;

  std::vector<cryptauthv2::ClientMetadata>
      client_metadata_from_enrollment_requests_;
  std::vector<std::optional<cryptauthv2::PolicyReference>>
      policy_references_from_enrollment_requests_;
  base::WeakPtrFactory<FakeCryptAuthSchedulerEnrollmentDelegate>
      weak_ptr_factory_{this};
};

// Fake CryptAuthScheduler::DeviceSyncDelegate implementation.
class FakeCryptAuthSchedulerDeviceSyncDelegate
    : public CryptAuthScheduler::DeviceSyncDelegate {
 public:
  FakeCryptAuthSchedulerDeviceSyncDelegate();

  FakeCryptAuthSchedulerDeviceSyncDelegate(
      const FakeCryptAuthSchedulerDeviceSyncDelegate&) = delete;
  FakeCryptAuthSchedulerDeviceSyncDelegate& operator=(
      const FakeCryptAuthSchedulerDeviceSyncDelegate&) = delete;

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
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_SCHEDULER_H_
