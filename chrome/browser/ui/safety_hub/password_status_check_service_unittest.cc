// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service.h"

#include <string>

#include "base/json/values_util.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestEmail[] = "user@gmail.com";
constexpr char kOrigin1[] = "https://example1.com/";
constexpr char kOrigin2[] = "https://example2.com/";
constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";
constexpr char16_t kUsername3[] = u"charlie";
constexpr char16_t kUsername4[] = u"dora";
constexpr char16_t kPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kPassword2[] = u"new_fnlsr4@cm^mls@fkspnsg3d";
constexpr char16_t kWeakPassword[] = u"1234";

using password_manager::BulkLeakCheckDelegateInterface;
using password_manager::BulkLeakCheckService;
using password_manager::BulkLeakCheckServiceInterface;
using password_manager::InsecureType;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile) {
  return static_cast<BulkLeakCheckService*>(
      BulkLeakCheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindLambdaForTesting([identity_manager](
                                                  content::BrowserContext*) {
            return std::unique_ptr<
                KeyedService>(std::make_unique<BulkLeakCheckService>(
                identity_manager,
                base::MakeRefCounted<network::TestSharedURLLoaderFactory>()));
          })));
}

// Mock observer for BulkLeakCheckService for EXPECT_CALL.
class MockObserver : public BulkLeakCheckService::Observer {
 public:
  explicit MockObserver(BulkLeakCheckService* leak_check_service)
      : leak_check_service_(leak_check_service) {
    leak_check_service_->AddObserver(this);
  }

  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;

  ~MockObserver() override { leak_check_service_->RemoveObserver(this); }

  MOCK_METHOD(void,
              OnStateChanged,
              (BulkLeakCheckService::State state),
              (override));
  MOCK_METHOD(void,
              OnCredentialDone,
              (const LeakCheckCredential& credential, IsLeaked is_leaked),
              (override));

 private:
  raw_ptr<BulkLeakCheckService> leak_check_service_;
};

PasswordForm MakeForm(base::StringPiece16 username,
                      base::StringPiece16 password,
                      std::string origin = kOrigin1,
                      bool is_leaked = false) {
  PasswordForm form;
  form.username_value = username;
  form.password_value = password;
  form.signon_realm = origin;
  form.url = GURL(origin);

  if (is_leaked) {
    // Credential issues for weak and reused are detected automatically and
    // don't need to be specified explicitly.
    form.password_issues.insert_or_assign(
        InsecureType::kLeaked,
        password_manager::InsecurityMetadata(
            base::Time::Now(), password_manager::IsMuted(false),
            password_manager::TriggerBackendNotification(false)));
  }
  return form;
}

PasswordForm WeakForm() {
  return MakeForm(kUsername1, kWeakPassword);
}

PasswordForm LeakedForm() {
  return MakeForm(kUsername2, kPassword, kOrigin1, true);
}

PasswordForm ReusedForm1() {
  return MakeForm(kUsername3, kPassword);
}

PasswordForm ReusedForm2() {
  return MakeForm(kUsername4, kPassword, kOrigin2);
}

}  // namespace

class PasswordStatusCheckServiceBaseTest : public testing::Test {
 public:
  void CreateService() {
    service_ = std::make_unique<PasswordStatusCheckService>(&profile_);
    RunUntilIdle();
  }

  void UpdateInsecureCredentials() {
    service()->UpdateInsecureCredentialCountAsync();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }
  TestingProfile& profile() { return profile_; }
  PasswordStatusCheckService* service() { return service_.get(); }
  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }
  raw_ptr<BulkLeakCheckService> bulk_leak_check_service() {
    return bulk_leak_check_service_;
  }
  content::BrowserTaskEnvironment* task_environment() { return &task_env_; }

 private:
  void SetUp() override {
    CreateService();
    ExpectInfrastructureUninitialized();
  }

  void TearDown() override { ExpectInfrastructureUninitialized(); }

  void ExpectInfrastructureUninitialized() {
    EXPECT_FALSE(service()->GetSavedPasswordsPresenterForTesting());
    EXPECT_FALSE(service()->GetPasswordCheckDelegateForTesting());
    EXPECT_FALSE(service()->IsObservingSavedPasswordsPresenterForTesting());
    EXPECT_FALSE(service()->IsObservingBulkLeakCheckForTesting());
    EXPECT_FALSE(service()->is_password_check_running());
    EXPECT_FALSE(service()->is_update_credential_count_pending());
  }

  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;

  scoped_refptr<TestPasswordStore> profile_store_ =
      CreateAndUseTestPasswordStore(&profile_);

  scoped_refptr<TestPasswordStore> account_store_ =
      CreateAndUseTestAccountPasswordStore(&profile_);

  raw_ptr<BulkLeakCheckService> bulk_leak_check_service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);

  std::unique_ptr<PasswordStatusCheckService> service_;
};

class PasswordStatusCheckServiceParameterizedIssueTest
    : public PasswordStatusCheckServiceBaseTest,
      public testing::WithParamInterface<
          testing::tuple</*include_weak*/ bool,
                         /*include_compromised*/ bool,
                         /*include_reused*/ bool>> {
 public:
  bool include_weak() const { return std::get<0>(GetParam()); }
  bool include_compromised() const { return std::get<1>(GetParam()); }
  bool include_reused() const { return std::get<2>(GetParam()); }
};

class PasswordStatusCheckServiceParameterizedStoreTest
    : public PasswordStatusCheckServiceBaseTest,
      public testing::WithParamInterface</*use_profile_store*/ bool> {
 public:
  TestPasswordStore& password_store() {
    return GetParam() ? profile_store() : account_store();
  }
};

TEST_F(PasswordStatusCheckServiceBaseTest, NoIssuesInitially) {
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->weak_credential_count(), 0UL);
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
  EXPECT_EQ(service()->reused_credential_count(), 0UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedIssueTest,
       DetectIssuesDuringConstruction) {
  // Based on test parameters, add different credential issues to the store.
  if (include_weak()) {
    profile_store().AddLogin(WeakForm());
  }
  if (include_compromised()) {
    profile_store().AddLogin(LeakedForm());
  }
  if (include_reused()) {
    profile_store().AddLogin(ReusedForm1());
    profile_store().AddLogin(ReusedForm2());
  }

  // Service is restarted and existing issues are found during construction.
  CreateService();
  EXPECT_EQ(service()->weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service()->compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service()->reused_credential_count(), include_reused() ? 2UL : 0UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedIssueTest,
       DetectIssuesWhileActive) {
  // Based on test parameters, add different credential issues to the store.
  if (include_weak()) {
    profile_store().AddLogin(WeakForm());
  }
  if (include_compromised()) {
    profile_store().AddLogin(LeakedForm());
  }
  if (include_reused()) {
    profile_store().AddLogin(ReusedForm1());
    profile_store().AddLogin(ReusedForm2());
  }

  // Expect to find credential issues that were added while service is active
  // when updating the count.
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service()->compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service()->reused_credential_count(), include_reused() ? 2UL : 0UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedIssueTest,
       DetectAddedAndRemovedIssuesAutomatically) {
  PasswordForm weak_form = WeakForm();
  PasswordForm leaked_form = LeakedForm();
  PasswordForm reused_form_1 = ReusedForm1();
  PasswordForm reused_form_2 = ReusedForm2();

  // Based on test parameters, add different credential issues to the store.
  if (include_weak()) {
    profile_store().AddLogin(weak_form);
  }
  if (include_compromised()) {
    profile_store().AddLogin(leaked_form);
  }
  if (include_reused()) {
    profile_store().AddLogin(reused_form_1);
    profile_store().AddLogin(reused_form_2);
  }
  RunUntilIdle();

  // Service is able to pick up on the new password issues automatically.
  EXPECT_EQ(service()->weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service()->compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service()->reused_credential_count(), include_reused() ? 2UL : 0UL);

  // Removing the credentials with the issues is also detected.
  profile_store().RemoveLogin(weak_form);
  profile_store().RemoveLogin(leaked_form);
  profile_store().RemoveLogin(reused_form_1);
  profile_store().RemoveLogin(reused_form_2);
  RunUntilIdle();

  EXPECT_EQ(service()->weak_credential_count(), 0UL);
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
  EXPECT_EQ(service()->reused_credential_count(), 0UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedStoreTest,
       DetectChangingWeakPassword) {
  password_store().AddLogin(MakeForm(kUsername1, kWeakPassword));
  RunUntilIdle();
  EXPECT_EQ(service()->weak_credential_count(), 1UL);

  // When changing the password for this credential from the weak password to a
  // stronger password, it is no longer counted as weak.
  password_store().UpdateLogin(MakeForm(kUsername1, kPassword));
  RunUntilIdle();
  EXPECT_EQ(service()->weak_credential_count(), 0UL);

  // When the strong password changes to a weak one is is counted as such.
  password_store().UpdateLogin(MakeForm(kUsername1, kWeakPassword));
  RunUntilIdle();
  EXPECT_EQ(service()->weak_credential_count(), 1UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedStoreTest,
       DetectChangingLeakedPassword) {
  password_store().AddLogin(MakeForm(kUsername2, kPassword, kOrigin1, true));
  RunUntilIdle();
  EXPECT_EQ(service()->compromised_credential_count(), 1UL);

  // When a leaked password is changed it is no longer leaked.
  password_store().UpdateLogin(MakeForm(kUsername2, kPassword2));
  RunUntilIdle();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  // If an existing credential get leaked again the service picks up on that.
  password_store().UpdateLogin(
      MakeForm(kUsername2, kPassword2, kOrigin1, true));
  RunUntilIdle();
  EXPECT_EQ(service()->compromised_credential_count(), 1UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedStoreTest,
       DetectChangingReusedPassword) {
  // Two credentials share the same password. The service counts them as reused.
  password_store().AddLogin(MakeForm(kUsername3, kPassword));
  password_store().AddLogin(MakeForm(kUsername4, kPassword, kOrigin2));
  RunUntilIdle();
  EXPECT_EQ(service()->reused_credential_count(), 2UL);

  // After changing one the reused passwords, there are now 0.
  password_store().UpdateLogin(MakeForm(kUsername3, kPassword2));
  RunUntilIdle();
  EXPECT_EQ(service()->reused_credential_count(), 0UL);

  // Changing a password to be the same as an existing one should be picked up.
  password_store().UpdateLogin(MakeForm(kUsername4, kPassword2, kOrigin2));
  RunUntilIdle();
  EXPECT_EQ(service()->reused_credential_count(), 2UL);
}

TEST_F(PasswordStatusCheckServiceBaseTest, RepeatedlyUpdatingDoesNotCrash) {
  for (int i = 0; i < 5; ++i) {
    service()->UpdateInsecureCredentialCountAsync();
    service()->StartRepeatedUpdates();
  }
  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest, PasswordCheckNoPasswords) {
  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));
  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest,
       PasswordCheckSignedOutWithPasswords) {
  profile_store().AddLogin(MakeForm(kUsername1, kPassword));

  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kSignedOut));
  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest, PasswordCheck_FindCompromised) {
  identity_test_env().MakeAccountAvailable(kTestEmail);

  // Store credential that has no issue associated with it.
  profile_store().AddLogin(MakeForm(kUsername1, kPassword));
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  // When leak check runs, mock the result for this credential coming back
  // positive.
  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  RunUntilIdle();

  bulk_leak_check_service()->set_state_and_notify(
      BulkLeakCheckService::State::kIdle);
  static_cast<BulkLeakCheckDelegateInterface*>(bulk_leak_check_service())
      ->OnFinishedCredential(LeakCheckCredential(kUsername1, kPassword),
                             IsLeaked(true));
  RunUntilIdle();

  // New leak is now picked up by service.
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 1UL);
}

TEST_F(PasswordStatusCheckServiceBaseTest, PasswordCheck_Error) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  profile_store().AddLogin(MakeForm(kUsername1, kPassword));

  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());

  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kServiceError));

  RunUntilIdle();

  bulk_leak_check_service()->set_state_and_notify(
      BulkLeakCheckService::State::kServiceError);

  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
}

TEST_F(PasswordStatusCheckServiceBaseTest, PrefInitialized) {
  EXPECT_TRUE(profile().GetPrefs()->HasPrefPath(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval));

  EXPECT_EQ(service()->GetScheduledPasswordCheckInterval(),
            features::kBackgroundPasswordCheckInterval.Get());

  EXPECT_GE(service()->GetScheduledPasswordCheckTime(), base::Time::Now());
  EXPECT_LT(service()->GetScheduledPasswordCheckTime(),
            base::Time::Now() + service()->GetScheduledPasswordCheckInterval());
}

// If interval changes, the scheduled time at which the password check runs
// should be recomputed when `StartRepeatedUpdates` runs.
TEST_F(PasswordStatusCheckServiceBaseTest, CheckTimeUpdatedOnIntervalChange) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params_before;
  params_before[features::kBackgroundPasswordCheckInterval.name] = "10d";
  feature_list.InitAndEnableFeatureWithParameters(features::kSafetyHub,
                                                  params_before);

  service()->StartRepeatedUpdates();

  base::TimeDelta interval_before =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_before = service()->GetScheduledPasswordCheckTime();

  base::FieldTrialParams params_after;
  params_after[features::kBackgroundPasswordCheckInterval.name] = "20d";
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(features::kSafetyHub,
                                                  params_after);

  service()->StartRepeatedUpdates();

  base::TimeDelta interval_after =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_after = service()->GetScheduledPasswordCheckTime();

  ASSERT_EQ(interval_before * 2, interval_after);
  ASSERT_NE(check_time_before, check_time_after);
}

TEST_F(PasswordStatusCheckServiceBaseTest,
       CheckTimeUpdatedAfterRunScheduledInTheFuture) {
  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  base::TimeDelta interval_before =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_before = service()->GetScheduledPasswordCheckTime();

  // Time passes in which the scheduled password check should have run (Password
  // check is scheduled in the time window of [Now, Now + Interval]).
  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));
  RunUntilIdle();

  // After password check is completed, the next one should be scheduled.
  base::TimeDelta interval_after =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_after = service()->GetScheduledPasswordCheckTime();

  ASSERT_EQ(interval_before, interval_after);
  ASSERT_EQ(check_time_before + interval_before, check_time_after);
}

TEST_F(PasswordStatusCheckServiceBaseTest,
       CheckTimeUpdatedAfterRunScheduledInThePast) {
  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  // Set scheduled time to be in the past.
  service()->SetPasswordCheckSchedulePrefsWithInterval(
      service()->GetScheduledPasswordCheckTime() -
      service()->GetScheduledPasswordCheckInterval());

  base::TimeDelta interval_before =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_before = service()->GetScheduledPasswordCheckTime();

  service()->StartRepeatedUpdates();

  // If the scheduled check time is in the past, it should run within an hour.
  task_environment()->AdvanceClock(base::Hours(1));

  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));
  RunUntilIdle();

  // After password check is completed, the next one should be scheduled.
  base::TimeDelta interval_after =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_after = service()->GetScheduledPasswordCheckTime();

  ASSERT_EQ(interval_before, interval_after);
  ASSERT_EQ(check_time_before + interval_before, check_time_after);
}

TEST_F(PasswordStatusCheckServiceBaseTest, ScheduledCheckRunsRepeatedly) {
  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());
  int runs = 10;

  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning))
      .Times(runs);
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle))
      .Times(runs);

  for (int i = 0; i < runs; ++i) {
    task_environment()->AdvanceClock(
        service()->GetScheduledPasswordCheckInterval());
    RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordStatusCheckServiceParameterizedIssueTest,
    ::testing::Combine(/*include_weak*/ ::testing::Bool(),
                       /*include_compromised*/ ::testing::Bool(),
                       /*include_reused*/ ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(AccountOrProfileStore,
                         PasswordStatusCheckServiceParameterizedStoreTest,
                         testing::Bool());
