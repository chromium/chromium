// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/device_sync/cryptauth_enroller.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/mock_sync_scheduler.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace ash {

namespace device_sync {

namespace {

// The GCM registration id from a successful registration.
const char kGCMRegistrationId[] = "aValid:gcm-Registration";

// A deprecated V3 GCM registration (which should prompt re-registration)
const char kDeprecatedGCMRegistrationId[] = "invalid-gcm-Registration";

// The user's persistent public key identifying the local device.
const char kUserPublicKey[] = "user public key";

// The initial "Now" time for testing.
const double kInitialTimeNowSeconds = 20000000;

// A later "Now" time for testing.
const double kLaterTimeNow = kInitialTimeNowSeconds + 30;

// The timestamp of a last successful enrollment that is still valid.
const double kLastEnrollmentTimeSeconds =
    kInitialTimeNowSeconds - (60 * 60 * 24 * 15);

// The timestamp of a last successful enrollment that is expired.
const double kLastExpiredEnrollmentTimeSeconds =
    kInitialTimeNowSeconds - (60 * 60 * 24 * 100);

// Mocks out the actual enrollment flow.
class MockCryptAuthEnroller : public CryptAuthEnroller {
 public:
  MockCryptAuthEnroller() {}

  MockCryptAuthEnroller(const MockCryptAuthEnroller&) = delete;
  MockCryptAuthEnroller& operator=(const MockCryptAuthEnroller&) = delete;

  ~MockCryptAuthEnroller() override {}

  MOCK_METHOD5(Enroll,
               void(const std::string& user_public_key,
                    const std::string& user_private_key,
                    const cryptauth::GcmDeviceInfo& device_info,
                    cryptauth::InvocationReason invocation_reason,
                    EnrollmentFinishedCallback callback));
};

// Creates MockCryptAuthEnroller instances, and allows expecations to be set
// before they are returned.
class MockCryptAuthEnrollerFactory : public CryptAuthEnrollerFactory {
 public:
  MockCryptAuthEnrollerFactory()
      : next_cryptauth_enroller_(new NiceMock<MockCryptAuthEnroller>()) {}

  MockCryptAuthEnrollerFactory(const MockCryptAuthEnrollerFactory&) = delete;
  MockCryptAuthEnrollerFactory& operator=(const MockCryptAuthEnrollerFactory&) =
      delete;

  ~MockCryptAuthEnrollerFactory() override {}

  // CryptAuthEnrollerFactory:
  std::unique_ptr<CryptAuthEnroller> CreateInstance() override {
    auto passed_cryptauth_enroller = std::move(next_cryptauth_enroller_);
    next_cryptauth_enroller_ =
        std::make_unique<NiceMock<MockCryptAuthEnroller>>();
    return passed_cryptauth_enroller;
  }

  MockCryptAuthEnroller* next_cryptauth_enroller() {
    return next_cryptauth_enroller_.get();
  }

 private:
  // Stores the next CryptAuthEnroller to be created.
  // Ownership is passed to the caller of |CreateInstance()|.
  std::unique_ptr<MockCryptAuthEnroller> next_cryptauth_enroller_;
};

// Harness for testing CryptAuthEnrollmentManager.
class TestCryptAuthEnrollmentManager : public CryptAuthEnrollmentManagerImpl {
 public:
  TestCryptAuthEnrollmentManager(
      base::Clock* clock,
      std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
      std::unique_ptr<multidevice::SecureMessageDelegate>
          secure_message_delegate,
      const cryptauth::GcmDeviceInfo& device_info,
      CryptAuthGCMManager* gcm_manager,
      PrefService* pref_service)
      : CryptAuthEnrollmentManagerImpl(clock,
                                       std::move(enroller_factory),
                                       std::move(secure_message_delegate),
                                       device_info,
                                       gcm_manager,
                                       pref_service),
        scoped_sync_scheduler_(new NiceMock<MockSyncScheduler>()),
        weak_sync_scheduler_factory_(scoped_sync_scheduler_.get()) {
    SetSyncSchedulerForTest(base::WrapUnique(scoped_sync_scheduler_.get()));
  }

  TestCryptAuthEnrollmentManager(const TestCryptAuthEnrollmentManager&) =
      delete;
  TestCryptAuthEnrollmentManager& operator=(
      const TestCryptAuthEnrollmentManager&) = delete;

  ~TestCryptAuthEnrollmentManager() override {}

  base::WeakPtr<MockSyncScheduler> GetSyncScheduler() {
    return weak_sync_scheduler_factory_.GetWeakPtr();
  }

 private:
  // Ownership is passed to |CryptAuthEnrollmentManager| super class when
  // |CreateSyncScheduler()| is called.
  raw_ptr<NiceMock<MockSyncScheduler>> scoped_sync_scheduler_;

  // Stores the pointer of |scoped_sync_scheduler_| after ownership is passed to
  // the super class.
  // This should be safe because the life-time this SyncScheduler will always be
  // within the life of the TestCryptAuthEnrollmentManager object.
  base::WeakPtrFactory<MockSyncScheduler> weak_sync_scheduler_factory_;
};

}  // namespace

class DeviceSyncCryptAuthEnrollmentManagerImplTest
    : public testing::Test,
      public CryptAuthEnrollmentManager::Observer {
 public:
  DeviceSyncCryptAuthEnrollmentManagerImplTest(
      const DeviceSyncCryptAuthEnrollmentManagerImplTest&) = delete;
  DeviceSyncCryptAuthEnrollmentManagerImplTest& operator=(
      const DeviceSyncCryptAuthEnrollmentManagerImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthEnrollmentManagerImplTest()
      : public_key_(kUserPublicKey),
        enroller_factory_(new MockCryptAuthEnrollerFactory()),
        secure_message_delegate_(new multidevice::FakeSecureMessageDelegate()),
        gcm_manager_(kGCMRegistrationId),
        enrollment_manager_(&clock_,
                            base::WrapUnique(enroller_factory_.get()),
                            base::WrapUnique(secure_message_delegate_.get()),
                            device_info_,
                            &gcm_manager_,
                            &pref_service_) {}

  // testing::Test:
  void SetUp() override {
    clock_.SetNow(
        base::Time::FromSecondsSinceUnixEpoch(kInitialTimeNowSeconds));
    enrollment_manager_.AddObserver(this);

    private_key_ =
        secure_message_delegate_->GetPrivateKeyForPublicKey(public_key_);
    secure_message_delegate_->set_next_public_key(public_key_);

    CryptAuthEnrollmentManagerImpl::RegisterPrefs(pref_service_.registry());
    pref_service_.SetUserPref(
        prefs::kCryptAuthEnrollmentIsRecoveringFromFailure,
        std::make_unique<base::Value>(false));
    pref_service_.SetUserPref(
        prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds,
        std::make_unique<base::Value>(kLastEnrollmentTimeSeconds));
    pref_service_.SetUserPref(
        prefs::kCryptAuthEnrollmentReason,
        std::make_unique<base::Value>(cryptauth::INVOCATION_REASON_UNKNOWN));
    pref_service_.Set(prefs::kCryptAuthEnrollmentUserPublicKey,
                      util::EncodeAsValueString(public_key_));
    pref_service_.Set(prefs::kCryptAuthEnrollmentUserPrivateKey,
                      util::EncodeAsValueString(private_key_));

    ON_CALL(*sync_scheduler(), GetStrategy())
        .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  }

  void TearDown() override { enrollment_manager_.RemoveObserver(this); }

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentStarted() override { OnEnrollmentStartedProxy(); }

  void OnEnrollmentFinished(bool success) override {
    // Simulate the scheduler changing strategies based on success or failure.
    SyncScheduler::Strategy new_strategy =
        SyncScheduler::Strategy::AGGRESSIVE_RECOVERY;
    ON_CALL(*sync_scheduler(), GetStrategy())
        .WillByDefault(Return(new_strategy));

    OnEnrollmentFinishedProxy(success);
  }

  MOCK_METHOD0(OnEnrollmentStartedProxy, void());
  MOCK_METHOD1(OnEnrollmentFinishedProxy, void(bool success));

  // Simulates firing the SyncScheduler to trigger an enrollment attempt.
  CryptAuthEnroller::EnrollmentFinishedCallback FireSchedulerForEnrollment(
      cryptauth::InvocationReason expected_invocation_reason) {
    CryptAuthEnroller::EnrollmentFinishedCallback completion_callback;
    EXPECT_CALL(
        *next_cryptauth_enroller(),
        Enroll(public_key_, private_key_, _, expected_invocation_reason, _))
        .WillOnce(MoveArg<4>(&completion_callback));

    auto sync_request = std::make_unique<SyncScheduler::SyncRequest>(
        enrollment_manager_.GetSyncScheduler());
    EXPECT_CALL(*this, OnEnrollmentStartedProxy());

    SyncScheduler::Delegate* delegate =
        static_cast<SyncScheduler::Delegate*>(&enrollment_manager_);
    delegate->OnSyncRequested(std::move(sync_request));

    return completion_callback;
  }

  MockSyncScheduler* sync_scheduler() {
    return enrollment_manager_.GetSyncScheduler().get();
  }

  MockCryptAuthEnroller* next_cryptauth_enroller() {
    return enroller_factory_->next_cryptauth_enroller();
  }

  // The expected persistent keypair.
  std::string public_key_;
  std::string private_key_;

  base::SimpleTestClock clock_;

  // Owned by |enrollment_manager_|.
  raw_ptr<MockCryptAuthEnrollerFactory, DanglingUntriaged> enroller_factory_;

  // Ownered by |enrollment_manager_|.
  raw_ptr<multidevice::FakeSecureMessageDelegate, DanglingUntriaged>
      secure_message_delegate_;

  cryptauth::GcmDeviceInfo device_info_;

  TestingPrefServiceSimple pref_service_;

  FakeCryptAuthGCMManager gcm_manager_;

  TestCryptAuthEnrollmentManager enrollment_manager_;
};

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, RegisterPrefs) {
  TestingPrefServiceSimple pref_service;
  CryptAuthEnrollmentManagerImpl::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds));
  EXPECT_TRUE(pref_service.FindPreference(
      prefs::kCryptAuthEnrollmentIsRecoveringFromFailure));
  EXPECT_TRUE(pref_service.FindPreference(prefs::kCryptAuthEnrollmentReason));
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, GetEnrollmentState) {
  enrollment_manager_.Start();

  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  EXPECT_FALSE(enrollment_manager_.IsRecoveringFromFailure());

  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  EXPECT_TRUE(enrollment_manager_.IsRecoveringFromFailure());

  base::TimeDelta time_to_next_sync = base::Minutes(60);
  ON_CALL(*sync_scheduler(), GetTimeToNextSync())
      .WillByDefault(Return(time_to_next_sync));
  EXPECT_EQ(time_to_next_sync, enrollment_manager_.GetTimeToNextAttempt());

  ON_CALL(*sync_scheduler(), GetSyncState())
      .WillByDefault(Return(SyncScheduler::SyncState::SYNC_IN_PROGRESS));
  EXPECT_TRUE(enrollment_manager_.IsEnrollmentInProgress());

  ON_CALL(*sync_scheduler(), GetSyncState())
      .WillByDefault(Return(SyncScheduler::SyncState::WAITING_FOR_REFRESH));
  EXPECT_FALSE(enrollment_manager_.IsEnrollmentInProgress());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, InitWithDefaultPrefs) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::FromSecondsSinceUnixEpoch(kInitialTimeNowSeconds));
  base::TimeDelta elapsed_time =
      clock.Now() - base::Time::FromSecondsSinceUnixEpoch(0);

  TestingPrefServiceSimple pref_service;
  CryptAuthEnrollmentManagerImpl::RegisterPrefs(pref_service.registry());

  TestCryptAuthEnrollmentManager enrollment_manager(
      &clock, std::make_unique<MockCryptAuthEnrollerFactory>(),
      std::make_unique<multidevice::FakeSecureMessageDelegate>(), device_info_,
      &gcm_manager_, &pref_service);

  EXPECT_CALL(
      *enrollment_manager.GetSyncScheduler(),
      Start(elapsed_time, SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  enrollment_manager.Start();

  EXPECT_FALSE(enrollment_manager.IsEnrollmentValid());
  EXPECT_TRUE(enrollment_manager.GetLastEnrollmentTime().is_null());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, InitWithExistingPrefs) {
  EXPECT_CALL(*sync_scheduler(),
              Start(clock_.Now() - base::Time::FromSecondsSinceUnixEpoch(
                                       kLastEnrollmentTimeSeconds),
                    SyncScheduler::Strategy::PERIODIC_REFRESH));

  enrollment_manager_.Start();
  EXPECT_TRUE(enrollment_manager_.IsEnrollmentValid());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(kLastEnrollmentTimeSeconds),
            enrollment_manager_.GetLastEnrollmentTime());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest,
       InitWithExpiredEnrollment) {
  pref_service_.SetUserPref(
      prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds,
      std::make_unique<base::Value>(kLastExpiredEnrollmentTimeSeconds));

  EXPECT_CALL(*sync_scheduler(),
              Start(clock_.Now() - base::Time::FromSecondsSinceUnixEpoch(
                                       kLastExpiredEnrollmentTimeSeconds),
                    SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));

  enrollment_manager_.Start();
  EXPECT_FALSE(enrollment_manager_.IsEnrollmentValid());
  EXPECT_EQ(
      base::Time::FromSecondsSinceUnixEpoch(kLastExpiredEnrollmentTimeSeconds),
      enrollment_manager_.GetLastEnrollmentTime());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, ForceEnrollment) {
  enrollment_manager_.Start();

  EXPECT_CALL(*sync_scheduler(), ForceSync());
  enrollment_manager_.ForceEnrollmentNow(
      cryptauth::INVOCATION_REASON_SERVER_INITIATED,
      std::nullopt /* session_id */);
  ASSERT_FALSE(gcm_manager_.registration_in_progress());

  auto completion_callback =
      FireSchedulerForEnrollment(cryptauth::INVOCATION_REASON_SERVER_INITIATED);

  clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(kLaterTimeNow));
  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(true));
  std::move(completion_callback).Run(true);
  EXPECT_EQ(clock_.Now(), enrollment_manager_.GetLastEnrollmentTime());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest,
       ForceEnrollmentDeprecatedRegistrationId) {
  // Simulate a situation where the user has an existing V3 GCM
  // registration and is now syncing for the first time post-migration.
  gcm_manager_.set_registration_id(kDeprecatedGCMRegistrationId);
  enrollment_manager_.Start();
  EXPECT_TRUE(enrollment_manager_.IsEnrollmentValid());

  // Trigger a sync request.
  EXPECT_CALL(*this, OnEnrollmentStartedProxy());
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  auto sync_request = std::make_unique<SyncScheduler::SyncRequest>(
      enrollment_manager_.GetSyncScheduler());
  static_cast<SyncScheduler::Delegate*>(&enrollment_manager_)
      ->OnSyncRequested(std::move(sync_request));

  // Unlike in the above test case, a deprecated GCM Registration Id
  // should prompt a new GCM registration with a valid Id.
  CryptAuthEnroller::EnrollmentFinishedCallback enrollment_callback;
  EXPECT_CALL(*next_cryptauth_enroller(),
              Enroll(public_key_, private_key_, _,
                     cryptauth::INVOCATION_REASON_PERIODIC, _))
      .WillOnce(MoveArg<4>(&enrollment_callback));
  ASSERT_TRUE(gcm_manager_.registration_in_progress());
  gcm_manager_.CompleteRegistration(kGCMRegistrationId);

  // Complete CryptAuth enrollment.
  ASSERT_FALSE(enrollment_callback.is_null());
  clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(kLaterTimeNow));
  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(true));
  std::move(enrollment_callback).Run(true);
  EXPECT_EQ(clock_.Now(), enrollment_manager_.GetLastEnrollmentTime());
  EXPECT_TRUE(enrollment_manager_.IsEnrollmentValid());

  // Check that CryptAuthEnrollmentManager returns the expected key-pair.
  EXPECT_EQ(public_key_, enrollment_manager_.GetUserPublicKey());
  EXPECT_EQ(private_key_, enrollment_manager_.GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest,
       EnrollmentFailsThenSucceeds) {
  enrollment_manager_.Start();
  base::Time old_enrollment_time = enrollment_manager_.GetLastEnrollmentTime();

  // The first periodic enrollment fails.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  auto completion_callback =
      FireSchedulerForEnrollment(cryptauth::INVOCATION_REASON_PERIODIC);
  clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(kLaterTimeNow));
  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(false));
  std::move(completion_callback).Run(false);
  EXPECT_EQ(old_enrollment_time, enrollment_manager_.GetLastEnrollmentTime());
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kCryptAuthEnrollmentIsRecoveringFromFailure));

  // The second recovery enrollment succeeds.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::AGGRESSIVE_RECOVERY));
  completion_callback =
      FireSchedulerForEnrollment(cryptauth::INVOCATION_REASON_FAILURE_RECOVERY);
  clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(kLaterTimeNow + 30));
  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(true));
  std::move(completion_callback).Run(true);
  EXPECT_EQ(clock_.Now(), enrollment_manager_.GetLastEnrollmentTime());
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kCryptAuthEnrollmentIsRecoveringFromFailure));
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest,
       EnrollmentSucceedsForFirstTime) {
  // Initialize |enrollment_manager_|.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  gcm_manager_.set_registration_id(std::string());
  pref_service_.ClearPref(prefs::kCryptAuthEnrollmentUserPublicKey);
  pref_service_.ClearPref(prefs::kCryptAuthEnrollmentUserPrivateKey);
  pref_service_.ClearPref(prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds);
  enrollment_manager_.Start();
  EXPECT_FALSE(enrollment_manager_.IsEnrollmentValid());

  // Trigger a sync request.
  EXPECT_CALL(*this, OnEnrollmentStartedProxy());
  auto sync_request = std::make_unique<SyncScheduler::SyncRequest>(
      enrollment_manager_.GetSyncScheduler());
  static_cast<SyncScheduler::Delegate*>(&enrollment_manager_)
      ->OnSyncRequested(std::move(sync_request));

  // Complete GCM registration successfully, and expect an enrollment.
  CryptAuthEnroller::EnrollmentFinishedCallback enrollment_callback;
  EXPECT_CALL(*next_cryptauth_enroller(),
              Enroll(public_key_, private_key_, _,
                     cryptauth::INVOCATION_REASON_INITIALIZATION, _))
      .WillOnce(MoveArg<4>(&enrollment_callback));
  ASSERT_TRUE(gcm_manager_.registration_in_progress());
  gcm_manager_.CompleteRegistration(kGCMRegistrationId);

  // Complete CryptAuth enrollment.
  ASSERT_FALSE(enrollment_callback.is_null());
  clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(kLaterTimeNow));
  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(true));
  std::move(enrollment_callback).Run(true);
  EXPECT_EQ(clock_.Now(), enrollment_manager_.GetLastEnrollmentTime());
  EXPECT_TRUE(enrollment_manager_.IsEnrollmentValid());

  // Check that CryptAuthEnrollmentManager returns the expected key-pair.
  EXPECT_EQ(public_key_, enrollment_manager_.GetUserPublicKey());
  EXPECT_EQ(private_key_, enrollment_manager_.GetUserPrivateKey());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, GCMRegistrationFails) {
  // Initialize |enrollment_manager_|.
  ON_CALL(*sync_scheduler(), GetStrategy())
      .WillByDefault(Return(SyncScheduler::Strategy::PERIODIC_REFRESH));
  gcm_manager_.set_registration_id(std::string());
  enrollment_manager_.Start();

  // Trigger a sync request.
  EXPECT_CALL(*this, OnEnrollmentStartedProxy());
  auto sync_request = std::make_unique<SyncScheduler::SyncRequest>(
      enrollment_manager_.GetSyncScheduler());
  static_cast<SyncScheduler::Delegate*>(&enrollment_manager_)
      ->OnSyncRequested(std::move(sync_request));

  // Complete GCM registration with failure.
  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(false));
  gcm_manager_.CompleteRegistration(std::string());
}

TEST_F(DeviceSyncCryptAuthEnrollmentManagerImplTest, ReenrollOnGCMPushMessage) {
  enrollment_manager_.Start();

  // Simulate receiving a GCM push message, forcing the device to re-enroll.
  gcm_manager_.PushReenrollMessage(std::nullopt /* session_id */,
                                   std::nullopt /* feature_type */);
  auto completion_callback =
      FireSchedulerForEnrollment(cryptauth::INVOCATION_REASON_SERVER_INITIATED);

  EXPECT_CALL(*this, OnEnrollmentFinishedProxy(true));
  std::move(completion_callback).Run(true);
}

}  // namespace device_sync

}  // namespace ash
