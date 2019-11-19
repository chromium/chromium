// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_scheduler_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/services/device_sync/value_string_encoding.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace device_sync {

namespace {

enum class NetworkConnectionStatus { kDisconnected, kConnecting, kConnected };

constexpr base::TimeDelta kZeroTimeDelta = base::TimeDelta::FromSeconds(0);
constexpr base::TimeDelta kImmediateRetryDelay =
    base::TimeDelta::FromMinutes(5);
const char kWifiServiceGuid[] = "wifiGuid";
const char kSessionId[] = "sessionId";

}  // namespace

class DeviceSyncCryptAuthSchedulerImplTest : public testing::Test {
 protected:
  DeviceSyncCryptAuthSchedulerImplTest() = default;

  ~DeviceSyncCryptAuthSchedulerImplTest() override = default;

  void SetUp() override {
    CryptAuthSchedulerImpl::RegisterPrefs(pref_service_.registry());
  }

  void CreateScheduler(
      const base::Optional<cryptauthv2::ClientDirective>&
          persisted_client_directive,
      const base::Optional<cryptauthv2::ClientMetadata>&
          persisted_enrollment_client_metadata,
      const base::Optional<base::Time>& persisted_last_enrollment_attempt_time,
      const base::Optional<base::Time>&
          persisted_last_successful_enrollment_time,
      const base::Optional<cryptauthv2::ClientMetadata>&
          persisted_device_sync_client_metadata,
      const base::Optional<base::Time>& persisted_last_device_sync_attempt_time,
      const base::Optional<base::Time>&
          persisted_last_successful_device_sync_time) {
    if (persisted_client_directive) {
      pref_service_.Set(prefs::kCryptAuthSchedulerClientDirective,
                        util::EncodeProtoMessageAsValueString(
                            &persisted_client_directive.value()));
    }

    if (persisted_enrollment_client_metadata) {
      pref_service_.Set(
          prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata,
          util::EncodeProtoMessageAsValueString(
              &persisted_enrollment_client_metadata.value()));
    }

    if (persisted_device_sync_client_metadata) {
      pref_service_.Set(
          prefs::kCryptAuthSchedulerNextDeviceSyncRequestClientMetadata,
          util::EncodeProtoMessageAsValueString(
              &persisted_device_sync_client_metadata.value()));
    }

    if (persisted_last_enrollment_attempt_time) {
      pref_service_.SetTime(prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime,
                            *persisted_last_enrollment_attempt_time);
    }

    if (persisted_last_device_sync_attempt_time) {
      pref_service_.SetTime(prefs::kCryptAuthSchedulerLastDeviceSyncAttemptTime,
                            *persisted_last_device_sync_attempt_time);
    }

    if (persisted_last_successful_enrollment_time) {
      pref_service_.SetTime(
          prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime,
          *persisted_last_successful_enrollment_time);
    }

    if (persisted_last_successful_device_sync_time) {
      pref_service_.SetTime(
          prefs::kCryptAuthSchedulerLastSuccessfulDeviceSyncTime,
          *persisted_last_successful_device_sync_time);
    }

    EXPECT_TRUE(!scheduler_);

    auto mock_enrollment_timer = std::make_unique<base::MockOneShotTimer>();
    mock_enrollment_timer_ = mock_enrollment_timer.get();

    auto mock_device_sync_timer = std::make_unique<base::MockOneShotTimer>();
    mock_device_sync_timer_ = mock_device_sync_timer.get();

    scheduler_ = CryptAuthSchedulerImpl::Factory::Get()->BuildInstance(
        &pref_service_, network_helper_.network_state_handler(), &test_clock_,
        std::move(mock_enrollment_timer), std::move(mock_device_sync_timer));

    VerifyLastEnrollmentAttemptTime(persisted_last_enrollment_attempt_time);
    VerifyLastDeviceSyncAttemptTime(persisted_last_device_sync_attempt_time);
    VerifyLastSuccessfulEnrollmentTime(
        persisted_last_successful_enrollment_time);
    VerifyLastSuccessfulDeviceSyncTime(
        persisted_last_successful_device_sync_time);
  }

  void AddDisconnectedWifiNetwork() {
    EXPECT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"" << kWifiServiceGuid << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateIdle << "\""
       << "}";

    wifi_network_service_path_ = network_helper_.ConfigureService(ss.str());
  }

  void SetWifiNetworkStatus(NetworkConnectionStatus connection_status) {
    std::string shill_connection_status;
    switch (connection_status) {
      case NetworkConnectionStatus::kDisconnected:
        shill_connection_status = shill::kStateIdle;
        break;
      case NetworkConnectionStatus::kConnecting:
        shill_connection_status = shill::kStateAssociation;
        break;
      case NetworkConnectionStatus::kConnected:
        shill_connection_status = shill::kStateReady;
        break;
    }

    network_helper_.SetServiceProperty(wifi_network_service_path_,
                                       shill::kStateProperty,
                                       base::Value(shill_connection_status));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveWifiNetwork() {
    EXPECT_TRUE(!wifi_network_service_path_.empty());

    network_helper_.service_test()->RemoveService(wifi_network_service_path_);
    base::RunLoop().RunUntilIdle();

    wifi_network_service_path_.clear();
  }

  void VerifyLastClientMetadataReceivedByEnrollmentDelegate(
      size_t total_received,
      const base::Optional<cryptauthv2::ClientMetadata>& last_received =
          base::nullopt) {
    VerifyLastClientMetadataReceivedByDelegate(
        fake_enrollment_delegate_.client_metadata_from_enrollment_requests(),
        total_received, last_received);
  }

  void VerifyLastClientMetadataReceivedByDeviceSyncDelegate(
      size_t total_received,
      const base::Optional<cryptauthv2::ClientMetadata>& last_received =
          base::nullopt) {
    VerifyLastClientMetadataReceivedByDelegate(
        fake_device_sync_delegate_.client_metadata_from_device_sync_requests(),
        total_received, last_received);
  }

  void VerifyLastPolicyReferenceReceivedByEnrollmentDelegate(
      size_t total_received,
      const base::Optional<cryptauthv2::PolicyReference>& last_received =
          base::nullopt) {
    EXPECT_EQ(total_received, fake_enrollment_delegate_
                                  .policy_references_from_enrollment_requests()
                                  .size());

    if (fake_enrollment_delegate_.policy_references_from_enrollment_requests()
            .empty())
      return;

    EXPECT_EQ(
        last_received.has_value(),
        fake_enrollment_delegate_.policy_references_from_enrollment_requests()
            .back()
            .has_value());

    if (fake_enrollment_delegate_.policy_references_from_enrollment_requests()
            .back()
            .has_value() &&
        last_received.has_value()) {
      EXPECT_EQ(
          last_received->SerializeAsString(),
          fake_enrollment_delegate_.policy_references_from_enrollment_requests()
              .back()
              ->SerializeAsString());
    }
  }

  void VerifyLastSuccessfulEnrollmentTime(
      const base::Optional<base::Time>& expected_time) {
    EXPECT_EQ(expected_time, scheduler_->GetLastSuccessfulEnrollmentTime());

    EXPECT_EQ(pref_service_.GetTime(
                  prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime),
              expected_time.value_or(base::Time()));
  }

  void VerifyLastSuccessfulDeviceSyncTime(
      const base::Optional<base::Time>& expected_time) {
    EXPECT_EQ(expected_time, scheduler_->GetLastSuccessfulDeviceSyncTime());

    EXPECT_EQ(pref_service_.GetTime(
                  prefs::kCryptAuthSchedulerLastSuccessfulDeviceSyncTime),
              expected_time.value_or(base::Time()));
  }

  void VerifyLastEnrollmentAttemptTime(
      const base::Optional<base::Time>& expected_time) {
    EXPECT_EQ(pref_service_.GetTime(
                  prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime),
              expected_time.value_or(base::Time()));
  }

  void VerifyLastDeviceSyncAttemptTime(
      const base::Optional<base::Time>& expected_time) {
    EXPECT_EQ(pref_service_.GetTime(
                  prefs::kCryptAuthSchedulerLastDeviceSyncAttemptTime),
              expected_time.value_or(base::Time()));
  }

  void VerifyClientDirective(
      const cryptauthv2::ClientDirective& expected_client_directive) {
    EXPECT_EQ(util::EncodeProtoMessageAsValueString(&expected_client_directive),
              *pref_service_.Get(prefs::kCryptAuthSchedulerClientDirective));
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                  expected_client_directive.checkin_delay_millis()),
              scheduler()->GetRefreshPeriod());
  }

  void VerifyScheduledEnrollment(
      const cryptauthv2::ClientMetadata& expected_scheduled_enrollment_request,
      const base::TimeDelta& expected_delay) {
    VerifyNextEnrollmentRequest(expected_scheduled_enrollment_request);
    EXPECT_TRUE(mock_enrollment_timer_->IsRunning());
    EXPECT_EQ(expected_delay, mock_enrollment_timer_->GetCurrentDelay());
    EXPECT_EQ(expected_delay, scheduler_->GetTimeToNextEnrollmentRequest());
    EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());
  }

  void VerifyScheduledDeviceSync(
      const cryptauthv2::ClientMetadata& expected_scheduled_enrollment_request,
      const base::TimeDelta& expected_delay) {
    VerifyNextDeviceSyncRequest(expected_scheduled_enrollment_request);
    EXPECT_TRUE(mock_device_sync_timer_->IsRunning());
    EXPECT_EQ(expected_delay, mock_device_sync_timer_->GetCurrentDelay());
    EXPECT_EQ(expected_delay, scheduler_->GetTimeToNextDeviceSyncRequest());
    EXPECT_FALSE(scheduler()->IsWaitingForDeviceSyncResult());
  }

  void VerifyNoEnrollmentsTriggeredButRequestQueued(
      const cryptauthv2::ClientMetadata& expected_enrollment_request) {
    VerifyNextEnrollmentRequest(expected_enrollment_request);
    EXPECT_FALSE(enrollment_timer()->IsRunning());
    EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());
    VerifyLastPolicyReferenceReceivedByEnrollmentDelegate(
        0 /* total_received */);
    VerifyLastClientMetadataReceivedByEnrollmentDelegate(
        0 /* total_received */);
  }

  void VerifyNoDeviceSyncsTriggeredButRequestQueued(
      const cryptauthv2::ClientMetadata& expected_device_sync_request) {
    VerifyNextDeviceSyncRequest(expected_device_sync_request);
    EXPECT_FALSE(device_sync_timer()->IsRunning());
    EXPECT_FALSE(scheduler()->IsWaitingForDeviceSyncResult());
    VerifyLastClientMetadataReceivedByDeviceSyncDelegate(
        0 /* total_received */);
  }

  base::WeakPtr<FakeCryptAuthSchedulerEnrollmentDelegate>
  fake_enrollment_delegate() {
    return fake_enrollment_delegate_.GetWeakPtr();
  }

  base::WeakPtr<FakeCryptAuthSchedulerDeviceSyncDelegate>
  fake_device_sync_delegate() {
    return fake_device_sync_delegate_.GetWeakPtr();
  }

  base::SimpleTestClock* clock() { return &test_clock_; }

  base::MockOneShotTimer* enrollment_timer() { return mock_enrollment_timer_; }

  base::MockOneShotTimer* device_sync_timer() {
    return mock_device_sync_timer_;
  }

  CryptAuthScheduler* scheduler() { return scheduler_.get(); }

 private:
  void VerifyLastClientMetadataReceivedByDelegate(
      const std::vector<cryptauthv2::ClientMetadata>& delegate_client_metadata,
      size_t total_received,
      const base::Optional<cryptauthv2::ClientMetadata>& last_received) {
    EXPECT_EQ(total_received, delegate_client_metadata.size());

    if (delegate_client_metadata.empty())
      return;

    EXPECT_TRUE(last_received);
    EXPECT_EQ(last_received->SerializeAsString(),
              delegate_client_metadata.back().SerializeAsString());
  }

  void VerifyNextEnrollmentRequest(
      const cryptauthv2::ClientMetadata& expected_enrollment_request) {
    EXPECT_EQ(
        util::EncodeProtoMessageAsValueString(&expected_enrollment_request),
        *pref_service_.Get(
            prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata));
  }

  void VerifyNextDeviceSyncRequest(
      const cryptauthv2::ClientMetadata& expected_device_sync_request) {
    EXPECT_EQ(
        util::EncodeProtoMessageAsValueString(&expected_device_sync_request),
        *pref_service_.Get(
            prefs::kCryptAuthSchedulerNextDeviceSyncRequestClientMetadata));
  }

  base::test::TaskEnvironment task_environment_;
  FakeCryptAuthSchedulerEnrollmentDelegate fake_enrollment_delegate_;
  FakeCryptAuthSchedulerDeviceSyncDelegate fake_device_sync_delegate_;
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  base::MockOneShotTimer* mock_enrollment_timer_;
  base::MockOneShotTimer* mock_device_sync_timer_;
  NetworkStateTestHelper network_helper_{
      false /* use_default_devices_and_services */};
  std::string wifi_network_service_path_;
  std::unique_ptr<CryptAuthScheduler> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthSchedulerImplTest);
};

TEST_F(DeviceSyncCryptAuthSchedulerImplTest,
       SuccessfulInitializationAndPeriodicEnrollments) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  const base::Time kStartTime = base::Time::FromDoubleT(1600600000);
  const base::Time kInitializationFinishTime =
      kStartTime + base::TimeDelta::FromSeconds(5);

  clock()->SetNow(kStartTime);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  // No enrollment has been scheduled yet.
  EXPECT_FALSE(enrollment_timer()->IsRunning());
  EXPECT_EQ(base::nullopt, scheduler()->GetTimeToNextEnrollmentRequest());

  EXPECT_FALSE(scheduler()->HasEnrollmentSchedulingStarted());
  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  EXPECT_TRUE(scheduler()->HasEnrollmentSchedulingStarted());

  // No successful enrollment has ever occurred; attempt immediately.
  cryptauthv2::ClientMetadata expected_scheduled_enrollment_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::INITIALIZATION,
          base::nullopt /* session_id */);
  VerifyScheduledEnrollment(expected_scheduled_enrollment_request,
                            kZeroTimeDelta /* expected_delay */);

  enrollment_timer()->Fire();
  EXPECT_TRUE(scheduler()->IsWaitingForEnrollmentResult());

  // There is no policy reference until CryptAuth sends one with a
  // ClientDirective.
  VerifyLastPolicyReferenceReceivedByEnrollmentDelegate(
      1 /* total_received */, base::nullopt /* last_received*/);
  VerifyLastClientMetadataReceivedByEnrollmentDelegate(
      1 /* total_received */, expected_scheduled_enrollment_request);

  clock()->SetNow(kInitializationFinishTime);
  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      cryptauthv2::GetClientDirectiveForTest()));
  VerifyLastEnrollmentAttemptTime(kInitializationFinishTime);
  VerifyLastSuccessfulEnrollmentTime(kInitializationFinishTime);
  VerifyClientDirective(cryptauthv2::GetClientDirectiveForTest());

  // A periodic enrollment attempt is now scheduled.
  expected_scheduled_enrollment_request = cryptauthv2::BuildClientMetadata(
      0 /* retry_count */, cryptauthv2::ClientMetadata::PERIODIC,
      base::nullopt /* session_id */);
  VerifyScheduledEnrollment(
      expected_scheduled_enrollment_request,
      scheduler()->GetRefreshPeriod() /* expected_delay */);

  base::Time periodic_fired_time =
      kInitializationFinishTime + scheduler()->GetRefreshPeriod();
  clock()->SetNow(periodic_fired_time);
  enrollment_timer()->Fire();

  VerifyLastPolicyReferenceReceivedByEnrollmentDelegate(
      2 /* total_received */,
      cryptauthv2::GetClientDirectiveForTest().policy_reference());
  VerifyLastClientMetadataReceivedByEnrollmentDelegate(
      2 /* total_received */, expected_scheduled_enrollment_request);

  // Assume no new ClientDirective was received from CryptAuth this time;
  // scheduler continues to use last-known ClientDirective.
  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
      base::nullopt /* client_directive */));
  VerifyLastEnrollmentAttemptTime(periodic_fired_time);
  VerifyLastSuccessfulEnrollmentTime(periodic_fired_time);
  VerifyClientDirective(cryptauthv2::GetClientDirectiveForTest());
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest, FailedRequests) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  CreateScheduler(
      cryptauthv2::GetClientDirectiveForTest() /* persisted_client_directive */,
      base::nullopt /* persisted_enrollment_client_metadata */,
      base::nullopt /* persisted_last_enrollment_attempt_time */,
      base::nullopt /* persisted_last_successful_enrollment_time */,
      base::nullopt /* persisted_device_sync_client_metadata */,
      base::nullopt /* persisted_last_device_sync_attempt_time */,
      base::nullopt /* persisted_last_successful_device_sync_time */
  );

  // Queue up manual requests before scheduler starts.
  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::MANUAL,
                                 base::nullopt /* session_id */);
  scheduler()->RequestDeviceSync(cryptauthv2::ClientMetadata::MANUAL,
                                 base::nullopt /* session_id */);

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  cryptauthv2::ClientMetadata expected_request =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::MANUAL,
                                       base::nullopt /* session_id */);
  VerifyScheduledEnrollment(expected_request,
                            kZeroTimeDelta /* expected_delay */);
  VerifyScheduledDeviceSync(expected_request,
                            kZeroTimeDelta /* expected_delay */);

  // After using all immediate failure retry attempts allotted by the client
  // directive, schedule retries using client directive's retry_period_millis.
  for (int attempt = 1;
       attempt <= cryptauthv2::GetClientDirectiveForTest().retry_attempts() + 3;
       ++attempt) {
    enrollment_timer()->Fire();
    VerifyLastPolicyReferenceReceivedByEnrollmentDelegate(
        attempt /* total_received */,
        cryptauthv2::GetClientDirectiveForTest().policy_reference());
    VerifyLastClientMetadataReceivedByEnrollmentDelegate(
        attempt /* total_received */, expected_request);

    device_sync_timer()->Fire();
    VerifyLastClientMetadataReceivedByDeviceSyncDelegate(
        attempt /* total_received */, expected_request);

    scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
        CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
        base::nullopt /* client_directive */));
    scheduler()->HandleDeviceSyncResult(
        CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                      kErrorSyncMetadataApiCallBadRequest,
                                  false /* device_registry_changed */,
                                  base::nullopt /* client_directive */));

    // Verify the next scheduled Enrollment/DeviceSync. At this point, note that
    // the number of failed attempts == |attempt| == retry count of the next
    // request.
    expected_request.set_retry_count(attempt);
    base::TimeDelta expected_delay =
        attempt < cryptauthv2::GetClientDirectiveForTest().retry_attempts()
            ? kImmediateRetryDelay
            : base::TimeDelta::FromMilliseconds(
                  cryptauthv2::GetClientDirectiveForTest()
                      .retry_period_millis());
    VerifyScheduledEnrollment(expected_request, expected_delay);
    VerifyScheduledDeviceSync(expected_request, expected_delay);
  }
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest,
       RequestsNotScheduledUntilSchedulerStarted) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::MANUAL,
                                 kSessionId);
  scheduler()->RequestDeviceSync(cryptauthv2::ClientMetadata::MANUAL,
                                 kSessionId);

  cryptauthv2::ClientMetadata expected_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::MANUAL, kSessionId);
  VerifyNoEnrollmentsTriggeredButRequestQueued(expected_request);
  VerifyNoDeviceSyncsTriggeredButRequestQueued(expected_request);

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  VerifyScheduledEnrollment(expected_request,
                            kZeroTimeDelta /* expected_delay */);
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());
  VerifyScheduledDeviceSync(expected_request,
                            kZeroTimeDelta /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest,
       PendingRequestsScheduledAfterCurrentAttemptFinishes) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  // Start server-initiated attempts.
  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::SERVER_INITIATED,
                                 kSessionId);
  enrollment_timer()->Fire();
  scheduler()->RequestDeviceSync(cryptauthv2::ClientMetadata::SERVER_INITIATED,
                                 kSessionId);
  device_sync_timer()->Fire();

  // Make requests while attempts are in progress.
  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::MANUAL,
                                 base::nullopt /* session_id */);
  EXPECT_FALSE(enrollment_timer()->IsRunning());
  scheduler()->RequestDeviceSync(cryptauthv2::ClientMetadata::MANUAL,
                                 base::nullopt /* session_id */);
  EXPECT_FALSE(device_sync_timer()->IsRunning());

  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      cryptauthv2::GetClientDirectiveForTest()));
  scheduler()->HandleDeviceSyncResult(
      CryptAuthDeviceSyncResult(CryptAuthDeviceSyncResult::ResultCode::
                                    kErrorSyncMetadataApiCallBadRequest,
                                false /* device_registry_changed */,
                                cryptauthv2::GetClientDirectiveForTest()));

  // Pending request scheduled after current attempt finishes, even if it fails.
  cryptauthv2::ClientMetadata expected_request =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::MANUAL,
                                       base::nullopt /* session_id */);
  VerifyScheduledEnrollment(expected_request,
                            kZeroTimeDelta /* expected_delay */);
  VerifyScheduledDeviceSync(expected_request,
                            kZeroTimeDelta /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest, ScheduledRequestOverwritten) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::SERVER_INITIATED,
                                 kSessionId);
  scheduler()->RequestDeviceSync(cryptauthv2::ClientMetadata::SERVER_INITIATED,
                                 kSessionId);

  cryptauthv2::ClientMetadata expected_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          kSessionId);
  VerifyScheduledEnrollment(expected_request,
                            kZeroTimeDelta /* expected_delay */);
  VerifyScheduledDeviceSync(expected_request,
                            kZeroTimeDelta /* expected_delay */);

  // New requests made before the timers fires overwrite existing requests.
  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::MANUAL,
                                 base::nullopt /* session_id */);
  scheduler()->RequestDeviceSync(cryptauthv2::ClientMetadata::MANUAL,
                                 base::nullopt /* session_id */);

  expected_request = cryptauthv2::BuildClientMetadata(
      0 /* retry_count */, cryptauthv2::ClientMetadata::MANUAL,
      base::nullopt /* session_id */);
  VerifyScheduledEnrollment(expected_request,
                            kZeroTimeDelta /* expected_delay */);
  VerifyScheduledDeviceSync(expected_request,
                            kZeroTimeDelta /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest,
       ScheduleFailurePersistedRequestsOnStartUp) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  const base::Time kLastEnrollmentTime = base::Time::FromDoubleT(1600600000);
  const base::Time kLastEnrollmentAttemptTime =
      kLastEnrollmentTime + base::TimeDelta::FromDays(30);
  const base::Time kStartTime =
      kLastEnrollmentAttemptTime + (kImmediateRetryDelay / 2);

  clock()->SetNow(kStartTime);

  cryptauthv2::ClientMetadata persisted_enrollment_request =
      cryptauthv2::BuildClientMetadata(5 /* retry_count */,
                                       cryptauthv2::ClientMetadata::PERIODIC,
                                       base::nullopt /* session_id */);
  cryptauthv2::ClientMetadata persisted_device_sync_request =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::MANUAL,
                                       base::nullopt /* session_id */);

  CreateScheduler(
      cryptauthv2::GetClientDirectiveForTest() /* persisted_client_directive */,
      persisted_enrollment_request /* persisted_enrollment_client_metadata */,
      kLastEnrollmentAttemptTime /* persisted_last_enrollment_attempt_time */,
      kLastEnrollmentTime /* persisted_last_successful_enrollment_time */,
      persisted_device_sync_request /* persisted_device_sync_client_metadata */,
      base::nullopt /* persisted_last_device_sync_attempt_time */,
      base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  // Failure recovery retry count is reset to 1 on start-up so quick retry is
  // triggered.
  EXPECT_GT(cryptauthv2::GetClientDirectiveForTest().retry_attempts(), 0);
  persisted_enrollment_request.set_retry_count(1);
  VerifyScheduledEnrollment(persisted_enrollment_request,
                            kImmediateRetryDelay / 2 /* expected_delay */);

  VerifyScheduledDeviceSync(persisted_device_sync_request,
                            kZeroTimeDelta /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest, HandleInvokeNext) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  scheduler()->RequestEnrollment(cryptauthv2::ClientMetadata::MANUAL,
                                 kSessionId);
  enrollment_timer()->Fire();

  cryptauthv2::ClientDirective client_directive =
      cryptauthv2::GetClientDirectiveForTest();
  cryptauthv2::InvokeNext* invoke_next = client_directive.add_invoke_next();
  invoke_next->set_service(cryptauthv2::TARGET_SERVICE_UNSPECIFIED);
  invoke_next = client_directive.add_invoke_next();
  invoke_next->set_service(cryptauthv2::ENROLLMENT);
  invoke_next = client_directive.add_invoke_next();
  invoke_next->set_service(cryptauthv2::DEVICE_SYNC);

  // Failed attempt will not process InvokeNext;
  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      client_directive));
  VerifyScheduledEnrollment(
      cryptauthv2::BuildClientMetadata(
          1 /* retry_count */, cryptauthv2::ClientMetadata::MANUAL, kSessionId),
      kImmediateRetryDelay /* expected_delay */);
  EXPECT_FALSE(device_sync_timer()->IsRunning());

  enrollment_timer()->Fire();

  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      client_directive));

  cryptauthv2::ClientMetadata expected_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          kSessionId);
  VerifyScheduledEnrollment(expected_request,
                            kZeroTimeDelta /* expected_delay */);
  VerifyScheduledDeviceSync(expected_request,
                            kZeroTimeDelta /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest,
       UpdateTimersWithNewClientDirective) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  const base::Time kNow = base::Time::FromDoubleT(1600600000);
  clock()->SetNow(kNow);

  cryptauthv2::ClientMetadata expected_device_sync_request =
      cryptauthv2::BuildClientMetadata(
          1 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          kSessionId);

  cryptauthv2::ClientDirective old_client_directive =
      cryptauthv2::GetClientDirectiveForTest();

  CreateScheduler(
      old_client_directive /* persisted_client_directive */,
      base::nullopt /* persisted_enrollment_client_metadata */,
      kNow /* persisted_last_enrollment_attempt_time */,
      kNow /* persisted_last_successful_enrollment_time */,
      expected_device_sync_request /* persisted_device_sync_client_metadata */,
      kNow /* persisted_last_device_sync_attempt_time */,
      base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  cryptauthv2::ClientMetadata expected_enrollment_request =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::PERIODIC,
                                       base::nullopt /* session_id */);
  VerifyScheduledEnrollment(
      expected_enrollment_request,
      base::TimeDelta::FromMilliseconds(
          old_client_directive.checkin_delay_millis()) /* expected_delay */);

  VerifyScheduledDeviceSync(expected_device_sync_request,
                            kImmediateRetryDelay /* expected_delay */);

  enrollment_timer()->Fire();

  const int64_t kNewCheckinDelayMillis =
      old_client_directive.checkin_delay_millis() + 5000;
  const int64_t kNewRetryPeriodMillis =
      old_client_directive.retry_period_millis() + 8000;
  cryptauthv2::ClientDirective new_client_directive = old_client_directive;
  new_client_directive.set_checkin_delay_millis(kNewCheckinDelayMillis);
  new_client_directive.set_retry_attempts(0);
  new_client_directive.set_retry_period_millis(kNewRetryPeriodMillis);

  scheduler()->HandleEnrollmentResult(CryptAuthEnrollmentResult(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      new_client_directive));
  VerifyScheduledEnrollment(
      expected_enrollment_request,
      base::TimeDelta::FromMilliseconds(
          new_client_directive.checkin_delay_millis()) /* expected_delay */);
  VerifyScheduledDeviceSync(
      expected_device_sync_request,
      base::TimeDelta::FromMilliseconds(
          new_client_directive.retry_period_millis()) /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest, RequestsMadeWhileOffline) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kDisconnected);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  cryptauthv2::ClientMetadata expected_enrollment_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::INITIALIZATION,
          base::nullopt /* session_id */);
  VerifyScheduledEnrollment(expected_enrollment_request,
                            kZeroTimeDelta /* expected_delay */);

  cryptauthv2::ClientMetadata expected_device_sync_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          base::nullopt /* session_id */);
  scheduler()->RequestDeviceSync(
      expected_device_sync_request.invocation_reason(),
      expected_device_sync_request.session_id());
  VerifyScheduledDeviceSync(expected_device_sync_request,
                            kZeroTimeDelta /* expected_delay */);

  enrollment_timer()->Fire();
  VerifyNoEnrollmentsTriggeredButRequestQueued(expected_enrollment_request);
  device_sync_timer()->Fire();
  VerifyNoDeviceSyncsTriggeredButRequestQueued(expected_device_sync_request);

  SetWifiNetworkStatus(NetworkConnectionStatus::kConnecting);
  VerifyNoEnrollmentsTriggeredButRequestQueued(expected_enrollment_request);
  VerifyNoDeviceSyncsTriggeredButRequestQueued(expected_device_sync_request);

  // Once Wifi network connected, reschedule enrollment.
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);
  VerifyScheduledEnrollment(expected_enrollment_request,
                            kZeroTimeDelta /* expected_delay */);
  VerifyScheduledDeviceSync(expected_device_sync_request,
                            kZeroTimeDelta /* expected_delay */);

  enrollment_timer()->Fire();
  device_sync_timer()->Fire();

  EXPECT_TRUE(scheduler()->IsWaitingForEnrollmentResult());
  VerifyLastPolicyReferenceReceivedByEnrollmentDelegate(
      1 /* total_received */, base::nullopt /* last_received*/);
  VerifyLastClientMetadataReceivedByEnrollmentDelegate(
      1 /* total_received */, expected_enrollment_request);

  EXPECT_TRUE(scheduler()->IsWaitingForDeviceSyncResult());
  VerifyLastClientMetadataReceivedByDeviceSyncDelegate(
      1 /* total_received */, expected_device_sync_request);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest, RequestsMadeWithNoWifiNetwork) {
  const base::Time kNow = base::Time::FromDoubleT(1600600000);
  clock()->SetNow(kNow);
  cryptauthv2::ClientMetadata expected_enrollment_request =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::PERIODIC,
                                       base::nullopt /* session_id */);
  cryptauthv2::ClientMetadata expected_device_sync_request =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::MANUAL,
                                       base::nullopt /* session_id */);
  CreateScheduler(
      cryptauthv2::GetClientDirectiveForTest() /* persisted_client_directive */,
      expected_enrollment_request /* persisted_enrollment_client_metadata */,
      kNow /* persisted_last_enrollment_attempt_time */,
      kNow /* persisted_last_successful_enrollment_time */,
      expected_device_sync_request /* persisted_device_sync_client_metadata */,
      kNow /* persisted_last_device_sync_attempt_time */,
      kNow /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  VerifyScheduledEnrollment(expected_enrollment_request,
                            scheduler()->GetRefreshPeriod());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());
  VerifyScheduledDeviceSync(expected_device_sync_request,
                            kZeroTimeDelta /* expected_delay */);

  enrollment_timer()->Fire();
  VerifyNoEnrollmentsTriggeredButRequestQueued(expected_enrollment_request);
  device_sync_timer()->Fire();
  VerifyNoDeviceSyncsTriggeredButRequestQueued(expected_device_sync_request);

  // Once Wifi network connected, reschedule enrollment.
  const base::TimeDelta kTimeElapsed = base::TimeDelta::FromHours(10);
  clock()->SetNow(kNow + kTimeElapsed);
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);
  VerifyScheduledEnrollment(expected_enrollment_request,
                            scheduler()->GetRefreshPeriod() - kTimeElapsed);
  VerifyScheduledDeviceSync(expected_device_sync_request,
                            kZeroTimeDelta /* expected_delay */);
}

TEST_F(DeviceSyncCryptAuthSchedulerImplTest,
       RequestsScheduledAndWifiNetworkRemoved) {
  AddDisconnectedWifiNetwork();
  SetWifiNetworkStatus(NetworkConnectionStatus::kConnected);

  CreateScheduler(base::nullopt /* persisted_client_directive */,
                  base::nullopt /* persisted_enrollment_client_metadata */,
                  base::nullopt /* persisted_last_enrollment_attempt_time */,
                  base::nullopt /* persisted_last_successful_enrollment_time */,
                  base::nullopt /* persisted_device_sync_client_metadata */,
                  base::nullopt /* persisted_last_device_sync_attempt_time */,
                  base::nullopt /* persisted_last_successful_device_sync_time */
  );

  scheduler()->StartEnrollmentScheduling(fake_enrollment_delegate());
  scheduler()->StartDeviceSyncScheduling(fake_device_sync_delegate());

  cryptauthv2::ClientMetadata expected_enrollment_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::INITIALIZATION,
          base::nullopt /* session_id */);
  VerifyScheduledEnrollment(expected_enrollment_request,
                            kZeroTimeDelta /* expected_delay */);

  cryptauthv2::ClientMetadata expected_device_sync_request =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          base::nullopt /* session_id */);
  scheduler()->RequestDeviceSync(
      expected_device_sync_request.invocation_reason(),
      expected_device_sync_request.session_id());
  VerifyScheduledDeviceSync(expected_device_sync_request,
                            kZeroTimeDelta /* expected_delay */);

  RemoveWifiNetwork();

  enrollment_timer()->Fire();
  VerifyNoEnrollmentsTriggeredButRequestQueued(expected_enrollment_request);
  device_sync_timer()->Fire();
  VerifyNoDeviceSyncsTriggeredButRequestQueued(expected_device_sync_request);
}

}  // namespace device_sync

}  // namespace chromeos
