// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_v2_enrollment_manager_impl.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enroller.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enroller_impl.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_v2_enroller.h"
#include "chromeos/ash/services/device_sync/mock_cryptauth_client.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_feature_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_v2_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_constants.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kFakeV1PublicKey[] = "public_key_v1";
const char kFakeV1PrivateKey[] = "private_key_v1";
const char kFakeV2PublicKey[] = "public_key_v2";
const char kFakeV2PrivateKey[] = "private_key_v2";
const char kFakeSessionId[] = "session_id";

constexpr base::TimeDelta kFakeRefreshPeriod = base::Milliseconds(2592000000);
constexpr base::TimeDelta kFakeRetryPeriod = base::Milliseconds(43200000);

const cryptauthv2::PolicyReference& GetClientDirectivePolicyReferenceForTest() {
  static const base::NoDestructor<cryptauthv2::PolicyReference>
      policy_reference([] {
        return cryptauthv2::BuildPolicyReference("policy_reference_name",
                                                 1 /* version */);
      }());

  return *policy_reference;
}

class FakeCryptAuthV2EnrollerFactory : public CryptAuthV2EnrollerImpl::Factory {
 public:
  FakeCryptAuthV2EnrollerFactory(
      const CryptAuthKeyRegistry* expected_key_registry,
      const CryptAuthClientFactory* expected_client_factory)
      : expected_key_registry_(expected_key_registry),
        expected_client_factory_(expected_client_factory) {}

  FakeCryptAuthV2EnrollerFactory(const FakeCryptAuthV2EnrollerFactory&) =
      delete;
  FakeCryptAuthV2EnrollerFactory& operator=(
      const FakeCryptAuthV2EnrollerFactory&) = delete;

  ~FakeCryptAuthV2EnrollerFactory() override = default;

  const std::vector<raw_ptr<FakeCryptAuthV2Enroller, VectorExperimental>>&
  created_instances() {
    return created_instances_;
  }

 private:
  // CryptAuthV2EnrollerImpl::Factory:
  std::unique_ptr<CryptAuthV2Enroller> CreateInstance(
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer =
          std::make_unique<base::MockOneShotTimer>()) override {
    EXPECT_EQ(expected_key_registry_, key_registry);
    EXPECT_EQ(expected_client_factory_, client_factory);

    auto instance = std::make_unique<FakeCryptAuthV2Enroller>();
    created_instances_.push_back(instance.get());

    return instance;
  }

  raw_ptr<const CryptAuthKeyRegistry> expected_key_registry_;
  raw_ptr<const CryptAuthClientFactory> expected_client_factory_;

  std::vector<raw_ptr<FakeCryptAuthV2Enroller, VectorExperimental>>
      created_instances_;
};

}  // namespace

class DeviceSyncCryptAuthV2EnrollmentManagerImplTest
    : public testing::Test,
      CryptAuthEnrollmentManager::Observer {
 protected:
  DeviceSyncCryptAuthV2EnrollmentManagerImplTest()
      : fake_gcm_manager_(cryptauthv2::kTestGcmRegistrationId),
        mock_client_factory_(
            MockCryptAuthClientFactory::MockType::MAKE_NICE_MOCKS) {}

  // testing::Test:
  void SetUp() override {
    test_clock_.SetNow(base::Time::UnixEpoch());

    CryptAuthV2EnrollmentManagerImpl::RegisterPrefs(
        test_pref_service_.registry());
    CryptAuthKeyRegistryImpl::RegisterPrefs(test_pref_service_.registry());

    key_registry_ =
        CryptAuthKeyRegistryImpl::Factory::Create(&test_pref_service_);

    fake_enroller_factory_ = std::make_unique<FakeCryptAuthV2EnrollerFactory>(
        key_registry_.get(), &mock_client_factory_);
    CryptAuthV2EnrollerImpl::Factory::SetFactoryForTesting(
        fake_enroller_factory_.get());

    // Fix the scheduler's ClientDirective PolicyReference to test that it is
    // passed to the enroller properly.
    fake_enrollment_scheduler_.set_client_directive_policy_reference(
        GetClientDirectivePolicyReferenceForTest());
  }

  // testing::Test:
  void TearDown() override {
    if (enrollment_manager_)
      enrollment_manager_->RemoveObserver(this);

    CryptAuthV2EnrollerImpl::Factory::SetFactoryForTesting(nullptr);
  }

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentStarted() override {
    ++num_enrollment_started_notifications_;
  }
  void OnEnrollmentFinished(bool success) override {
    observer_enrollment_finished_success_list_.push_back(success);
  }

  void AddV1UserKeyPairToV1Prefs(const std::string& public_key,
                                 const std::string& private_key) {
    test_pref_service_.Set(prefs::kCryptAuthEnrollmentUserPublicKey,
                           util::EncodeAsValueString(public_key));
    test_pref_service_.Set(prefs::kCryptAuthEnrollmentUserPrivateKey,
                           util::EncodeAsValueString(private_key));
  }

  void CreateEnrollmentManager(
      const cryptauthv2::ClientAppMetadata& client_app_metadata) {
    VerifyUserKeyPairStateHistogram(num_manager_creations_ /* total_count */);

    enrollment_manager_ = CryptAuthV2EnrollmentManagerImpl::Factory::Create(
        client_app_metadata, key_registry_.get(), &mock_client_factory_,
        &fake_gcm_manager_, &fake_enrollment_scheduler_, &test_pref_service_,
        &test_clock_);
    ++num_manager_creations_;

    VerifyUserKeyPairStateHistogram(num_manager_creations_ /* total_count */);

    enrollment_manager_->AddObserver(this);
  }

  void DestroyEnrollmentManager() { enrollment_manager_.reset(); }

  void RequestEnrollmentThroughGcm(
      const std::optional<std::string>& session_id) {
    fake_gcm_manager_.PushReenrollMessage(session_id,
                                          std::nullopt /* feature_type */);
  }

  void VerifyEnrollmentManagerObserversNotifiedOfStart(
      size_t expected_num_enrollment_started_notifications) {
    EXPECT_EQ(expected_num_enrollment_started_notifications,
              num_enrollment_started_notifications_);
  }

  void FinishEnrollmentAttempt(
      size_t expected_enroller_instance_index,
      const cryptauthv2::ClientMetadata& expected_client_metadata,
      const cryptauthv2::ClientAppMetadata& expected_client_app_metadata,
      const CryptAuthEnrollmentResult& enrollment_result) {
    EXPECT_TRUE(enrollment_manager_->IsEnrollmentInProgress());

    // Only the most recently created enroller is valid.
    EXPECT_EQ(fake_enroller_factory_->created_instances().size() - 1,
              expected_enroller_instance_index);
    FakeCryptAuthV2Enroller* enroller =
        fake_enroller_factory_
            ->created_instances()[expected_enroller_instance_index];

    VerifyEnrollerData(enroller, expected_client_metadata,
                       expected_client_app_metadata);

    enroller->FinishAttempt(enrollment_result);

    EXPECT_FALSE(enrollment_manager_->IsEnrollmentInProgress());
  }

  void VerifyEnrollmentResults(
      const std::vector<CryptAuthEnrollmentResult>& expected_results) {
    VerifyResultsSentToEnrollmentManagerObservers(expected_results);
    VerifyResultsSentToScheduler(expected_results);
    VerifyEnrollmentResultHistograms(expected_results);
  }

  void VerifyInvocationReasonHistogram(
      const std::vector<cryptauthv2::ClientMetadata::InvocationReason>&
          expected_invocation_reasons) {
    std::unordered_map<cryptauthv2::ClientMetadata::InvocationReason, size_t>
        reason_to_count_map;
    for (const auto& reason : expected_invocation_reasons)
      ++reason_to_count_map[reason];

    size_t total_count = 0;
    for (const auto& reason_count_pair : reason_to_count_map) {
      histogram_tester_.ExpectBucketCount(
          "CryptAuth.EnrollmentV2.InvocationReason", reason_count_pair.first,
          reason_count_pair.second);
      total_count += reason_count_pair.second;
    }

    histogram_tester_.ExpectTotalCount(
        "CryptAuth.EnrollmentV2.InvocationReason", total_count);
  }

  void VerifyUserKeyPairStateHistogram(size_t total_count) {
    histogram_tester_.ExpectTotalCount(
        "CryptAuth.EnrollmentV2.UserKeyPairState", total_count);
  }

  CryptAuthKeyRegistry* key_registry() { return key_registry_.get(); }

  FakeCryptAuthScheduler* fake_enrollment_scheduler() {
    return &fake_enrollment_scheduler_;
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

  const base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  CryptAuthEnrollmentManager* enrollment_manager() {
    return enrollment_manager_.get();
  }

 private:
  void VerifyEnrollerData(
      FakeCryptAuthV2Enroller* enroller,
      const cryptauthv2::ClientMetadata& expected_client_metadata,
      const cryptauthv2::ClientAppMetadata& expected_client_app_metadata) {
    EXPECT_TRUE(enroller->was_enroll_called());
    EXPECT_EQ(expected_client_metadata.SerializeAsString(),
              enroller->client_metadata()->SerializeAsString());
    EXPECT_EQ(expected_client_app_metadata.SerializeAsString(),
              enroller->client_app_metadata()->SerializeAsString());
    EXPECT_EQ(
        GetClientDirectivePolicyReferenceForTest().SerializeAsString(),
        (*enroller->client_directive_policy_reference())->SerializeAsString());
  }

  void VerifyResultsSentToEnrollmentManagerObservers(
      const std::vector<CryptAuthEnrollmentResult>
          expected_enrollment_results) {
    ASSERT_EQ(expected_enrollment_results.size(),
              observer_enrollment_finished_success_list_.size());

    for (size_t i = 0; i < expected_enrollment_results.size(); ++i) {
      EXPECT_EQ(expected_enrollment_results[i].IsSuccess(),
                observer_enrollment_finished_success_list_[i]);
    }
  }

  void VerifyResultsSentToScheduler(const std::vector<CryptAuthEnrollmentResult>
                                        expected_enrollment_results) {
    EXPECT_EQ(expected_enrollment_results,
              fake_enrollment_scheduler()->handled_enrollment_results());
  }

  void VerifyEnrollmentResultHistograms(
      const std::vector<CryptAuthEnrollmentResult>
          expected_enrollment_results) {
    std::unordered_map<CryptAuthEnrollmentResult::ResultCode, size_t>
        result_code_to_count_map;
    for (const auto& result : expected_enrollment_results)
      ++result_code_to_count_map[result.result_code()];

    size_t success_count = 0;
    size_t failure_count = 0;
    for (const auto& result_count_pair : result_code_to_count_map) {
      histogram_tester_.ExpectBucketCount(
          "CryptAuth.EnrollmentV2.Result.ResultCode", result_count_pair.first,
          result_count_pair.second);

      if (CryptAuthEnrollmentResult(result_count_pair.first, std::nullopt)
              .IsSuccess()) {
        success_count += result_count_pair.second;
      } else {
        failure_count += result_count_pair.second;
      }
    }

    histogram_tester_.ExpectTotalCount(
        "CryptAuth.EnrollmentV2.Result.ResultCode",
        success_count + failure_count);
    histogram_tester_.ExpectBucketCount("CryptAuth.EnrollmentV2.Result.Success",
                                        true, success_count);
    histogram_tester_.ExpectBucketCount("CryptAuth.EnrollmentV2.Result.Success",
                                        false, failure_count);
  }

  size_t num_manager_creations_ = 0;
  size_t num_enrollment_started_notifications_ = 0;
  std::vector<bool> observer_enrollment_finished_success_list_;

  TestingPrefServiceSimple test_pref_service_;
  FakeCryptAuthGCMManager fake_gcm_manager_;
  FakeCryptAuthScheduler fake_enrollment_scheduler_;
  base::SimpleTestClock test_clock_;
  MockCryptAuthClientFactory mock_client_factory_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
  std::unique_ptr<FakeCryptAuthV2EnrollerFactory> fake_enroller_factory_;

  std::unique_ptr<CryptAuthEnrollmentManager> enrollment_manager_;
};

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       EnrollmentRequestedFromScheduler_NeverPreviouslyEnrolled) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  EXPECT_FALSE(fake_enrollment_scheduler()->HasEnrollmentSchedulingStarted());

  enrollment_manager()->Start();
  EXPECT_TRUE(fake_enrollment_scheduler()->HasEnrollmentSchedulingStarted());

  // The user has never enrolled with v1 or v2.
  fake_enrollment_scheduler()->set_last_successful_enrollment_time(
      base::Time());
  EXPECT_TRUE(key_registry()->key_bundles().empty());
  EXPECT_TRUE(enrollment_manager()->GetLastEnrollmentTime().is_null());
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentValid());

  // Scheduler triggers an initialization enrollment request.
  fake_enrollment_scheduler()->set_num_consecutive_enrollment_failures(0);
  fake_enrollment_scheduler()->RequestEnrollment(
      cryptauthv2::ClientMetadata::INITIALIZATION /* invocation_reason */,
      std::nullopt /* session_id */);

  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);

  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      cryptauthv2::GetClientDirectiveForTest());

  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::INITIALIZATION /* invocation_reason */,
          std::nullopt /* session_id */) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result);

  VerifyEnrollmentResults({expected_enrollment_result});
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, ForcedEnrollment) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  enrollment_manager()->Start();

  enrollment_manager()->ForceEnrollmentNow(
      cryptauth::InvocationReason::INVOCATION_REASON_FEATURE_TOGGLED,
      kFakeSessionId);
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);

  // Simulate a failed enrollment attempt due to CryptAuth server overload.
  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      std::nullopt /* client_directive */);

  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::FEATURE_TOGGLED /* invocation_reason */,
          kFakeSessionId) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result);

  VerifyEnrollmentResults({expected_enrollment_result});
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       RetryAfterFailedPeriodicEnrollment_PreviouslyEnrolled) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());

  // The user has already enrolled.
  CryptAuthKey user_key_pair_v2(
      kFakeV2PublicKey, kFakeV2PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                         user_key_pair_v2);
  base::Time expected_last_enrollment_time = test_clock()->Now();
  fake_enrollment_scheduler()->set_last_successful_enrollment_time(
      expected_last_enrollment_time);
  EXPECT_EQ(expected_last_enrollment_time,
            enrollment_manager()->GetLastEnrollmentTime());
  fake_enrollment_scheduler()->set_refresh_period(kFakeRefreshPeriod);
  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      kFakeRefreshPeriod);
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentValid());

  enrollment_manager()->Start();

  EXPECT_EQ(kFakeRefreshPeriod, enrollment_manager()->GetTimeToNextAttempt());

  // Now, user is due for a refresh.
  test_clock()->SetNow(test_clock()->Now() + kFakeRefreshPeriod +
                       base::Seconds(1));
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentValid());
  base::TimeDelta expected_time_to_next_attempt = base::Seconds(0);
  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      expected_time_to_next_attempt);
  EXPECT_EQ(expected_time_to_next_attempt,
            enrollment_manager()->GetTimeToNextAttempt());

  cryptauthv2::ClientMetadata expected_client_metadata =
      cryptauthv2::BuildClientMetadata(0 /* retry_count */,
                                       cryptauthv2::ClientMetadata::PERIODIC,
                                       std::nullopt /* session_id */);

  // First enrollment attempt fails.
  // Note: User does not yet have a GCM registration ID or ClientAppMetadata.
  fake_enrollment_scheduler()->RequestEnrollment(
      cryptauthv2::ClientMetadata::PERIODIC, std::nullopt /* session_id */);
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);

  CryptAuthEnrollmentResult first_expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      std::nullopt /* client_directive */);

  FinishEnrollmentAttempt(0u /* expected_enroller_instance_index */,
                          expected_client_metadata,
                          cryptauthv2::GetClientAppMetadataForTest(),
                          first_expected_enrollment_result);

  EXPECT_FALSE(enrollment_manager()->IsRecoveringFromFailure());
  fake_enrollment_scheduler()->set_num_consecutive_enrollment_failures(1);
  EXPECT_TRUE(enrollment_manager()->IsRecoveringFromFailure());

  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      kFakeRetryPeriod);
  EXPECT_EQ(kFakeRetryPeriod, enrollment_manager()->GetTimeToNextAttempt());

  // Second (successful) enrollment attempt bypasses GCM registration and
  // ClientAppMetadata fetch because they were performed during the failed
  // attempt.
  test_clock()->SetNow(test_clock()->Now() + kFakeRetryPeriod);
  fake_enrollment_scheduler()->RequestEnrollment(
      cryptauthv2::ClientMetadata::PERIODIC, std::nullopt /* session_id */);
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      2 /* expected_num_enrollment_started_notifications */);

  CryptAuthEnrollmentResult second_expected_enrollment_result =
      CryptAuthEnrollmentResult(
          CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
          cryptauthv2::GetClientDirectiveForTest());

  expected_client_metadata.set_retry_count(1);
  FinishEnrollmentAttempt(1u /* expected_enroller_instance_index */,
                          expected_client_metadata,
                          cryptauthv2::GetClientAppMetadataForTest(),
                          second_expected_enrollment_result);

  VerifyEnrollmentResults(
      {first_expected_enrollment_result, second_expected_enrollment_result});

  fake_enrollment_scheduler()->set_last_successful_enrollment_time(
      test_clock()->Now());
  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      kFakeRefreshPeriod);
  fake_enrollment_scheduler()->set_num_consecutive_enrollment_failures(0);
  EXPECT_EQ(test_clock()->Now(), enrollment_manager()->GetLastEnrollmentTime());
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentValid());
  EXPECT_EQ(kFakeRefreshPeriod, enrollment_manager()->GetTimeToNextAttempt());
  EXPECT_FALSE(enrollment_manager()->IsRecoveringFromFailure());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       EnrollmentTriggeredByGcmMessage_Success) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  enrollment_manager()->Start();

  RequestEnrollmentThroughGcm(kFakeSessionId);

  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);
  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
      cryptauthv2::GetClientDirectiveForTest());
  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          kFakeSessionId) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result);
  VerifyEnrollmentResults({expected_enrollment_result});
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       EnrollmentTriggeredByGcmMessage_Failure) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  enrollment_manager()->Start();

  RequestEnrollmentThroughGcm(kFakeSessionId);

  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);
  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      std::nullopt /* client_directive */);
  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::SERVER_INITIATED,
          kFakeSessionId) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result);
  VerifyEnrollmentResults({expected_enrollment_result});
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       V1UserKeyPairAddedToRegistryOnConstruction) {
  AddV1UserKeyPairToV1Prefs(kFakeV1PublicKey, kFakeV1PrivateKey);
  CryptAuthKey expected_user_key_pair_v1(
      kFakeV1PublicKey, kFakeV1PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);

  EXPECT_FALSE(
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));

  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  histogram_tester()->ExpectBucketCount(
      "CryptAuth.EnrollmentV2.UserKeyPairState",
      1 /* UserKeyPairState::kYesV1KeyNoV2Key */, 1 /* count */);

  EXPECT_EQ(
      expected_user_key_pair_v1,
      *key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));
  EXPECT_EQ(expected_user_key_pair_v1.public_key(),
            enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(expected_user_key_pair_v1.private_key(),
            enrollment_manager()->GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       V1UserKeyPairOverwritesV2UserKeyPairOnConstruction) {
  AddV1UserKeyPairToV1Prefs(kFakeV1PublicKey, kFakeV1PrivateKey);
  CryptAuthKey expected_user_key_pair_v1(
      kFakeV1PublicKey, kFakeV1PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);

  // Add v2 user key pair to registry.
  CryptAuthKey user_key_pair_v2(
      kFakeV2PublicKey, kFakeV2PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                         user_key_pair_v2);
  EXPECT_EQ(user_key_pair_v2, *key_registry()->GetActiveKey(
                                  CryptAuthKeyBundle::Name::kUserKeyPair));

  // A legacy v1 user key pair should overwrite any existing v2 user key pair
  // when the enrollment manager is constructed.
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  histogram_tester()->ExpectBucketCount(
      "CryptAuth.EnrollmentV2.UserKeyPairState",
      4 /* UserKeyPairState::kYesV1KeyYesV2KeyDisagree */, 1 /* count */);

  EXPECT_EQ(
      expected_user_key_pair_v1,
      *key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kUserKeyPair));
  EXPECT_EQ(expected_user_key_pair_v1.public_key(),
            enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(expected_user_key_pair_v1.private_key(),
            enrollment_manager()->GetUserPrivateKey());

  // Expect re-enrollment using newly added v1 key.
  enrollment_manager()->Start();
  cryptauthv2::ClientMetadata expected_client_metadata =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::INITIALIZATION,
          std::nullopt /* session_id */);
  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      std::nullopt /* client_directive */);
  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */, expected_client_metadata,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result);

  VerifyInvocationReasonHistogram(
      {expected_client_metadata.invocation_reason()});
  VerifyEnrollmentResults({expected_enrollment_result});
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       V1AndV2UserKeyPairsAgree) {
  AddV1UserKeyPairToV1Prefs(kFakeV1PublicKey, kFakeV1PrivateKey);
  CryptAuthKey user_key_pair_v1(
      kFakeV1PublicKey, kFakeV1PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);

  key_registry()->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                         user_key_pair_v1);

  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  histogram_tester()->ExpectBucketCount(
      "CryptAuth.EnrollmentV2.UserKeyPairState",
      3 /* UserKeyPairState::kYesV1KeyYesV2KeyAgree */, 1 /* count */);

  EXPECT_EQ(user_key_pair_v1, *key_registry()->GetActiveKey(
                                  CryptAuthKeyBundle::Name::kUserKeyPair));
  EXPECT_EQ(user_key_pair_v1.public_key(),
            enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(user_key_pair_v1.private_key(),
            enrollment_manager()->GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, V2KeyButNoV1Key) {
  CryptAuthKey user_key_pair_v2(
      kFakeV2PublicKey, kFakeV2PrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);

  key_registry()->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair,
                         user_key_pair_v2);

  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  histogram_tester()->ExpectBucketCount(
      "CryptAuth.EnrollmentV2.UserKeyPairState",
      2 /* UserKeyPairState::kNoV1KeyYesV2Key */, 1 /* count */);

  EXPECT_EQ(user_key_pair_v2, *key_registry()->GetActiveKey(
                                  CryptAuthKeyBundle::Name::kUserKeyPair));
  EXPECT_EQ(kFakeV2PublicKey, enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(kFakeV2PrivateKey, enrollment_manager()->GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest, GetUserKeyPair) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  histogram_tester()->ExpectBucketCount(
      "CryptAuth.EnrollmentV2.UserKeyPairState",
      0 /* UserKeyPairState::kNoV1KeyNoV2Key */, 1 /* count */);

  EXPECT_TRUE(enrollment_manager()->GetUserPublicKey().empty());
  EXPECT_TRUE(enrollment_manager()->GetUserPrivateKey().empty());

  key_registry()->AddKey(
      CryptAuthKeyBundle::Name::kUserKeyPair,
      CryptAuthKey(kFakeV2PublicKey, kFakeV2PrivateKey,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
                   kCryptAuthFixedUserKeyPairHandle));
  EXPECT_EQ(kFakeV2PublicKey, enrollment_manager()->GetUserPublicKey());
  EXPECT_EQ(kFakeV2PrivateKey, enrollment_manager()->GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       MultipleEnrollmentAttempts) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  enrollment_manager()->Start();

  std::vector<cryptauthv2::ClientMetadata::InvocationReason>
      expected_invocation_reasons;
  std::vector<CryptAuthEnrollmentResult> expected_enrollment_results;

  // Successfully enroll for the first time.
  expected_invocation_reasons.push_back(
      cryptauthv2::ClientMetadata::INITIALIZATION);
  expected_enrollment_results.emplace_back(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      cryptauthv2::GetClientDirectiveForTest());
  fake_enrollment_scheduler()->RequestEnrollment(
      expected_invocation_reasons.back(), kFakeSessionId);
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      1 /* expected_num_enrollment_started_notifications */);
  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, expected_invocation_reasons.back(),
          kFakeSessionId) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(),
      expected_enrollment_results.back());

  // Fail periodic refresh twice due to overloaded CryptAuth server.
  expected_invocation_reasons.push_back(cryptauthv2::ClientMetadata::PERIODIC);
  expected_enrollment_results.emplace_back(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      cryptauthv2::GetClientDirectiveForTest());
  fake_enrollment_scheduler()->RequestEnrollment(
      expected_invocation_reasons.back(), kFakeSessionId);
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      2 /* expected_num_enrollment_started_notifications */);
  FinishEnrollmentAttempt(
      1u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, expected_invocation_reasons.back(),
          kFakeSessionId) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(),
      expected_enrollment_results.back());
  fake_enrollment_scheduler()->set_num_consecutive_enrollment_failures(1);
  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      kFakeRetryPeriod);

  expected_invocation_reasons.push_back(cryptauthv2::ClientMetadata::PERIODIC);
  expected_enrollment_results.emplace_back(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      std::nullopt /* client_directive */);
  fake_enrollment_scheduler()->RequestEnrollment(
      expected_invocation_reasons.back(), kFakeSessionId);
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      3 /* expected_num_enrollment_started_notifications */);
  FinishEnrollmentAttempt(
      2u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          1 /* retry_count */, expected_invocation_reasons.back(),
          kFakeSessionId) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(),
      expected_enrollment_results.back());
  fake_enrollment_scheduler()->set_num_consecutive_enrollment_failures(2);
  fake_enrollment_scheduler()->set_time_to_next_enrollment_request(
      kFakeRetryPeriod);

  // While waiting for retry, force a manual enrollment that succeeds.
  expected_invocation_reasons.push_back(cryptauthv2::ClientMetadata::MANUAL);
  expected_enrollment_results.emplace_back(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
      std::nullopt /* client_directive */);
  enrollment_manager()->ForceEnrollmentNow(cryptauth::INVOCATION_REASON_MANUAL,
                                           std::nullopt /* session_id */);
  VerifyEnrollmentManagerObserversNotifiedOfStart(
      4 /* expected_num_enrollment_started_notifications */);
  FinishEnrollmentAttempt(
      3u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          2 /* retry_count */, expected_invocation_reasons.back(),
          std::nullopt /* session_id */) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(),
      expected_enrollment_results.back());

  VerifyInvocationReasonHistogram(expected_invocation_reasons);
  VerifyEnrollmentResults(expected_enrollment_results);
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       MissingUserKeyPairRecovery) {
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());

  // The user has already enrolled according to the scheduler, but there is no
  // user key pair because the key registry was corrupted, for instance.
  fake_enrollment_scheduler()->set_last_successful_enrollment_time(
      test_clock()->Now());
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentValid());

  enrollment_manager()->Start();

  cryptauthv2::ClientMetadata expected_client_metadata =
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */, cryptauthv2::ClientMetadata::FAILURE_RECOVERY,
          std::nullopt /* session_id */);
  CryptAuthEnrollmentResult expected_enrollment_result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      std::nullopt /* client_directive */);
  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */, expected_client_metadata,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result);

  VerifyInvocationReasonHistogram(
      {expected_client_metadata.invocation_reason()});
  VerifyEnrollmentResults({expected_enrollment_result});
}

TEST_F(DeviceSyncCryptAuthV2EnrollmentManagerImplTest,
       ClientAppMetadataChange) {
  // If the user has never enrolled, no forced enrollment should be made due to
  // a ClientAppMetadata change.
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  enrollment_manager()->Start();
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentInProgress());

  // Succeed initialization and persist the client app metadata
  fake_enrollment_scheduler()->RequestEnrollment(
      cryptauthv2::ClientMetadata::INITIALIZATION /* invocation_reason */,
      std::nullopt /* session_id */);
  CryptAuthEnrollmentResult expected_enrollment_result1(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      cryptauthv2::GetClientDirectiveForTest());
  FinishEnrollmentAttempt(
      0u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::INITIALIZATION /* invocation_reason */,
          std::nullopt /* session_id */) /* expected_client_metadata */,
      cryptauthv2::GetClientAppMetadataForTest(), expected_enrollment_result1);
  VerifyEnrollmentResults({expected_enrollment_result1});

  // The client app metadata doesn't change, so don't force an enrollment.
  DestroyEnrollmentManager();
  CreateEnrollmentManager(cryptauthv2::GetClientAppMetadataForTest());
  enrollment_manager()->Start();
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentInProgress());

  // The client app metadata changes, force an enrollment.
  DestroyEnrollmentManager();
  cryptauthv2::ClientAppMetadata new_client_app_metadata =
      cryptauthv2::GetClientAppMetadataForTest();
  new_client_app_metadata.set_instance_id("new_instance_id");
  CreateEnrollmentManager(new_client_app_metadata);
  enrollment_manager()->Start();
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());

  // Enrollment fails; new client app metadata not persisted.
  CryptAuthEnrollmentResult expected_enrollment_result2(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      std::nullopt /* client_directive */);
  FinishEnrollmentAttempt(
      1u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::SOFTWARE_UPDATE /* invocation_reason */,
          std::nullopt /* session_id */) /* expected_client_metadata */,
      new_client_app_metadata, expected_enrollment_result2);
  VerifyEnrollmentResults(
      {expected_enrollment_result1, expected_enrollment_result2});

  // We still haven't successfully enrolled the new client app metadata; on
  // start-up we still see a metadata change and force the enrollment.
  DestroyEnrollmentManager();
  CreateEnrollmentManager(new_client_app_metadata);
  enrollment_manager()->Start();
  EXPECT_TRUE(enrollment_manager()->IsEnrollmentInProgress());

  // Enrollment succeeds; new client app metadata is persisted.
  CryptAuthEnrollmentResult expected_enrollment_result3(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      std::nullopt /* client_directive */);
  FinishEnrollmentAttempt(
      2u /* expected_enroller_instance_index */,
      cryptauthv2::BuildClientMetadata(
          0 /* retry_count */,
          cryptauthv2::ClientMetadata::SOFTWARE_UPDATE /* invocation_reason */,
          std::nullopt /* session_id */) /* expected_client_metadata */,
      new_client_app_metadata, expected_enrollment_result3);
  VerifyEnrollmentResults({expected_enrollment_result1,
                           expected_enrollment_result2,
                           expected_enrollment_result3});

  // We have successfully enrolled the new client app metadata; no change
  // detected on start-up.
  DestroyEnrollmentManager();
  CreateEnrollmentManager(new_client_app_metadata);
  enrollment_manager()->Start();
  EXPECT_FALSE(enrollment_manager()->IsEnrollmentInProgress());
}

}  // namespace device_sync

}  // namespace ash
