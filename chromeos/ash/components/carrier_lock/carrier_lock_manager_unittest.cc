// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/carrier_lock_manager.h"
#include "chromeos/ash/components/carrier_lock/common.h"
#include "chromeos/ash/components/carrier_lock/fake_fcm_topic_subscriber.h"
#include "chromeos/ash/components/carrier_lock/fake_provisioning_config_fetcher.h"
#include "chromeos/ash/components/carrier_lock/fake_psm_claim_verifier.h"
#include "chromeos/ash/components/carrier_lock/metrics.h"

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/fake_network_3gpp_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/net_errors.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::carrier_lock {

namespace {
const char kTestTopic[] = "/topics/test";
const char kTestImei[] = "test_imei";
}  // namespace

class CarrierLockManagerTest : public testing::Test {
 public:
  CarrierLockManagerTest() = default;
  CarrierLockManagerTest(const CarrierLockManagerTest&) = delete;
  CarrierLockManagerTest& operator=(const CarrierLockManagerTest&) = delete;
  ~CarrierLockManagerTest() override = default;

  void RunManager();

 protected:
  // testing::Test:
  void SetUp() override {
    // Enable Carrier Lock feature flag
    scoped_feature_list_.InitAndEnableFeature(features::kCellularCarrierLock);

    pref_state_ = std::make_unique<TestingPrefServiceSimple>();
    ash::carrier_lock::CarrierLockManager::RegisterLocalPrefs(
        pref_state_->registry());
    fake_modem_handler_ = std::make_unique<FakeNetwork3gppHandler>();
    fake_config_fetcher_ = std::make_unique<FakeProvisioningConfigFetcher>();
    fake_psm_verifier_ = std::make_unique<FakePsmClaimVerifier>();
    fake_fcm_subscriber_ = std::make_unique<FakeFcmTopicSubscriber>();
  }

  void TearDown() override { carrier_lock_manager_.reset(); }

  std::unique_ptr<TestingPrefServiceSimple> pref_state_;
  std::unique_ptr<CarrierLockManager> carrier_lock_manager_;
  std::unique_ptr<FakeNetwork3gppHandler> fake_modem_handler_;
  std::unique_ptr<FakeFcmTopicSubscriber> fake_fcm_subscriber_;
  std::unique_ptr<FakePsmClaimVerifier> fake_psm_verifier_;
  std::unique_ptr<FakeProvisioningConfigFetcher> fake_config_fetcher_;
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
};

void CarrierLockManagerTest::RunManager() {
  // Set Manager Prefs to clean state
  pref_state_->SetTime(kLastConfigTimePref, base::Time());
  pref_state_->SetBoolean(kDisableManagerPref, false);
  pref_state_->SetString(kFcmTopicPref, std::string());
  pref_state_->SetString(kLastImeiPref, std::string());
  pref_state_->SetInteger(kErrorCounterPref, 0);

  carrier_lock_manager_ = carrier_lock::CarrierLockManager::CreateForTesting(
      pref_state_.get(), fake_modem_handler_.get(),
      std::move(fake_fcm_subscriber_), std::move(fake_psm_verifier_),
      std::move(fake_config_fetcher_));
  carrier_lock_manager_->imei_ = kTestImei;
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerUnlocked) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("UnlockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(true, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(0, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigUnlocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemNotLocked, 1);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 0);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 0,
                                       1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerNoPsmMember) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ false,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string(),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(true, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(0, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceUnlocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigUnlocked, 0);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemNotLocked, 0);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 0);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 0);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 0,
                                       0);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerLocked) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(kTestTopic), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(0, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemLocked, 1);
  histogram_tester_.ExpectBucketCount(kFcmCommunicationResult,
                                      FcmResult::kRegistered, 1);
  histogram_tester_.ExpectBucketCount(kFcmCommunicationResult,
                                      FcmResult::kSubscribed, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 0,
                                       0);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerTempUnlocked) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("UnlockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(kTestTopic), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(0, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(
      kProvisioningServerResponse, ProvisioningResult::kConfigTempUnlocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemLocked, 1);
  histogram_tester_.ExpectBucketCount(kFcmCommunicationResult,
                                      FcmResult::kRegistered, 1);
  histogram_tester_.ExpectBucketCount(kFcmCommunicationResult,
                                      FcmResult::kSubscribed, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 0,
                                       0);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerPsmFailed) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kConnectionError);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string(),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceUnlocked, 0);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 0);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemNotLocked, 0);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 0);
  histogram_tester_.ExpectUniqueSample(kErrorPsmClaim, Result::kConnectionError,
                                       1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerProvisioningFailed) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string(),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kConnectionError);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 0);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemNotLocked, 0);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 1);
  histogram_tester_.ExpectUniqueSample(kErrorProvisioning,
                                       Result::kConnectionError, 1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerModemFailed) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(
      CarrierLockResult::kUnknownError);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(kTestTopic), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemNotLocked, 0);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 1);
  histogram_tester_.ExpectUniqueSample(kErrorModemSetup,
                                       Result::kModemInternalError, 1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerFcmRegistrationFailed) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string(),
                                          /*result*/ Result::kConnectionError);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 0);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemLocked, 0);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 0);
  histogram_tester_.ExpectUniqueSample(kErrorFcmTopic, Result::kConnectionError,
                                       1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerFcmSubscriptionFailed) {
  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kConnectionError);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(kTestTopic), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 1);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemLocked, 1);
  histogram_tester_.ExpectUniqueSample(kFcmCommunicationResult,
                                       FcmResult::kRegistered, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 0,
                                       0);
  histogram_tester_.ExpectUniqueSample(kErrorFcmTopic, Result::kConnectionError,
                                       1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerLockedToUnlocked) {
  FakeProvisioningConfigFetcher* config = fake_config_fetcher_.get();
  FakeFcmTopicSubscriber* fcm = fake_fcm_subscriber_.get();

  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(0, pref_state_->GetInteger(kErrorCounterPref));

  // Trigger FCM notification to unlock device
  config->SetConfigTopicAndResult(
      /*configuration*/ std::string("UnlockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kSuccess);
  fcm->SendNotification();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(true, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(0, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectBucketCount(kModemConfigurationResult,
                                      ConfigurationResult::kModemLocked, 1);
  histogram_tester_.ExpectBucketCount(kModemConfigurationResult,
                                      ConfigurationResult::kModemUnlocked, 1);
  histogram_tester_.ExpectUniqueSample(kFcmNotificationType,
                                       FcmNotification::kUnlockDevice, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 0,
                                       1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerPsmRetry) {
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Set PSM to return error
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kConnectionError);

  // Run Carrier Lock Manager
  RunManager();

  // Wait until all retry attempts fail
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(kRetryDelay * (kMaxRetries + 1));
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(kTestTopic), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(kMaxRetries + 1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectUniqueSample(kPsmClaimResponse,
                                       PsmResult::kDeviceLocked, 0);
  histogram_tester_.ExpectUniqueSample(kProvisioningServerResponse,
                                       ProvisioningResult::kConfigLocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemLocked, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 3, 1);
  histogram_tester_.ExpectUniqueSample(kErrorPsmClaim, Result::kConnectionError,
                                       3);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerProvisioningRetry) {
  FakeProvisioningConfigFetcher* config = fake_config_fetcher_.get();

  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kSuccess);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  // Set invalid configuration
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_EQ(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Retry configuration fetch with unlocked config
  config->SetConfigTopicAndResult(
      /*configuration*/ std::string("UnlockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_ALLOW,
      /*fcm topic*/ std::string(),
      /*result*/ Result::kSuccess);
  task_environment_.FastForwardBy(kRetryDelay);
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(true, pref_state_->GetBoolean(kDisableManagerPref));

  // Check histograms
  histogram_tester_.ExpectBucketCount(kProvisioningServerResponse,
                                      ProvisioningResult::kConfigInvalid, 1);
  histogram_tester_.ExpectBucketCount(kProvisioningServerResponse,
                                      ProvisioningResult::kConfigUnlocked, 1);
  histogram_tester_.ExpectUniqueSample(kModemConfigurationResult,
                                       ConfigurationResult::kModemNotLocked, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeUnlock, 1,
                                       1);
  histogram_tester_.ExpectUniqueSample(kErrorProvisioning,
                                       Result::kLockedWithoutTopic, 1);
}

TEST_F(CarrierLockManagerTest, CarrierLockStartManagerSubscriptionRetry) {
  FakeFcmTopicSubscriber* fcm = fake_fcm_subscriber_.get();

  // Set return values for fake auxiliary classes
  fake_modem_handler_->set_carrier_lock_result(CarrierLockResult::kSuccess);
  fake_fcm_subscriber_->SetTokenAndResult(/*token*/ std::string("Token"),
                                          /*result*/ Result::kConnectionError);
  fake_psm_verifier_->SetMemberAndResult(/*membership*/ true,
                                         /*result*/ Result::kSuccess);
  fake_config_fetcher_->SetConfigTopicAndResult(
      /*configuration*/ std::string("LockedConfig"),
      /*restriction mode*/ ::carrier_lock::DEFAULT_DISALLOW,
      /*fcm topic*/ std::string(kTestTopic),
      /*result*/ Result::kSuccess);

  // Run Carrier Lock Manager
  RunManager();
  task_environment_.RunUntilIdle();

  // Retry topic subscription
  fcm->SetTokenAndResult(/*token*/ std::string("Token"),
                         /*result*/ Result::kSuccess);
  task_environment_.FastForwardBy(kRetryDelay);
  task_environment_.RunUntilIdle();

  // Check local pref values
  EXPECT_NE(base::Time(), pref_state_->GetTime(kLastConfigTimePref));
  EXPECT_EQ(std::string(kTestTopic), pref_state_->GetString(kFcmTopicPref));
  EXPECT_EQ(std::string(kTestImei), pref_state_->GetString(kLastImeiPref));
  EXPECT_EQ(false, pref_state_->GetBoolean(kDisableManagerPref));
  EXPECT_EQ(1, pref_state_->GetInteger(kErrorCounterPref));

  // Check histograms
  histogram_tester_.ExpectBucketCount(kFcmCommunicationResult,
                                      FcmResult::kSubscribed, 1);
  histogram_tester_.ExpectBucketCount(kFcmCommunicationResult,
                                      FcmResult::kRegistered, 1);
  histogram_tester_.ExpectUniqueSample(kNumConsecutiveFailuresBeforeLock, 0, 1);
  histogram_tester_.ExpectUniqueSample(kErrorFcmTopic, Result::kConnectionError,
                                       1);
}

}  // namespace ash::carrier_lock
