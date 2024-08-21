// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service.h"

#include <string>
#include <string_view>

#include "base/json/values_util.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/safety_hub/password_status_check_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
using safety_hub::SafetyHubCardState;
using safety_hub_test_util::MakeForm;

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

PasswordForm WeakForm() {
  return MakeForm(kUsername1, kWeakPassword, kOrigin1);
}

PasswordForm LeakedForm() {
  return MakeForm(kUsername2, kPassword, kOrigin1, true);
}

PasswordForm ReusedForm1() {
  return MakeForm(kUsername3, kPassword2, kOrigin1);
}

PasswordForm ReusedForm2() {
  return MakeForm(kUsername4, kPassword2, kOrigin2);
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

  void AdvanceClockForWeakAndReusedChecks() {
    // Weak and reuse passwords are not detected while adding password. Travel
    // in time one day to let weak and reuse checks run.
    task_environment()->AdvanceClock(base::Days(1));
    RunUntilIdle();
  }

  void SetLastCheckTime(base::TimeDelta time_ago) {
    base::Time check_time = base::Time::Now() - time_ago;
    profile().GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        check_time.InSecondsFSinceUnixEpoch());
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
    EXPECT_FALSE(service()->IsObservingBulkLeakCheckForTesting());
    EXPECT_FALSE(service()->is_password_check_running());
    EXPECT_FALSE(service()->is_running_weak_reused_check());
    EXPECT_FALSE(service()->GetInsecureCredentialsManagerForTesting());
    EXPECT_FALSE(service()->GetBulkLeakCheckServiceAdapterForTesting());
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
      safety_hub_test_util::CreateAndUseBulkLeakCheckService(
          identity_test_env_.identity_manager(),
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
  bool any_issue_included() const {
    return include_weak() || include_compromised() || include_reused();
  }
};

class PasswordStatusCheckServiceParameterizedStoreTest
    : public PasswordStatusCheckServiceBaseTest,
      public testing::WithParamInterface</*use_profile_store*/ bool> {
 public:
  TestPasswordStore& password_store() {
    return GetParam() ? profile_store() : account_store();
  }
};

class PasswordStatusCheckServiceParameterizedCardTest
    : public PasswordStatusCheckServiceBaseTest,
      public testing::WithParamInterface<
          testing::tuple</*include_compromised*/ bool,
                         /*include_weak*/ bool,
                         /*include_reused*/ bool,
                         /*check_ran_previously*/ bool,
                         /*signed_in*/ bool,
                         /*include_safe_password*/ bool,
                         /*password_saving_allowed*/ bool>> {
 public:
  int include_compromised() const { return std::get<0>(GetParam()); }
  int include_weak() const { return std::get<1>(GetParam()); }
  int include_reused() const { return std::get<2>(GetParam()); }
  bool check_ran_previously() const { return std::get<3>(GetParam()); }
  bool signed_in() const { return std::get<4>(GetParam()); }
  bool include_safe_password() const { return std::get<5>(GetParam()); }
  bool password_saving_allowed() const { return std::get<6>(GetParam()); }

  bool any_issue_included() const {
    return include_weak() || include_compromised() || include_reused();
  }
  bool any_password_saved() const {
    return any_issue_included() || include_safe_password();
  }
};

class PasswordStatusCheckServiceParameterizedSchedulingTest
    : public PasswordStatusCheckServiceBaseTest,
      public testing::WithParamInterface<int> {
 public:
  int GetCurrentWeekday() const { return GetParam(); }
  std::string GetWeightForDay(int day) const {
    return day == GetCurrentWeekday() ? "1" : "0";
  }
};

TEST_F(PasswordStatusCheckServiceBaseTest, NoIssuesInitially) {
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->weak_credential_count(), 0UL);
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
  EXPECT_EQ(service()->reused_credential_count(), 0UL);
  EXPECT_TRUE(service()->no_passwords_saved());
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
  EXPECT_EQ(any_issue_included(), !service()->no_passwords_saved());
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
  EXPECT_EQ(any_issue_included(), !service()->no_passwords_saved());
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

  AdvanceClockForWeakAndReusedChecks();
  // Service is able to pick up on the new password issues automatically.
  EXPECT_EQ(service()->weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service()->compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service()->reused_credential_count(), include_reused() ? 2UL : 0UL);

  // Removing the credentials with the issues is also detected.
  profile_store().RemoveLogin(FROM_HERE, weak_form);
  profile_store().RemoveLogin(FROM_HERE, leaked_form);
  profile_store().RemoveLogin(FROM_HERE, reused_form_1);
  profile_store().RemoveLogin(FROM_HERE, reused_form_2);

  AdvanceClockForWeakAndReusedChecks();
  EXPECT_EQ(service()->weak_credential_count(), 0UL);
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
  EXPECT_EQ(service()->reused_credential_count(), 0UL);
  EXPECT_TRUE(service()->no_passwords_saved());
}

TEST_P(PasswordStatusCheckServiceParameterizedStoreTest,
       DetectChangingWeakPassword) {
  password_store().AddLogin(MakeForm(kUsername1, kWeakPassword, kOrigin1));
  AdvanceClockForWeakAndReusedChecks();
  EXPECT_EQ(service()->weak_credential_count(), 1UL);

  // When changing the password for this credential from the weak password to a
  // stronger password, it is no longer counted as weak.
  password_store().UpdateLogin(MakeForm(kUsername1, kPassword, kOrigin1));
  AdvanceClockForWeakAndReusedChecks();
  EXPECT_EQ(service()->weak_credential_count(), 0UL);

  // When the strong password changes to a weak one is is counted as such.
  password_store().UpdateLogin(MakeForm(kUsername1, kWeakPassword, kOrigin1));
  AdvanceClockForWeakAndReusedChecks();
  EXPECT_EQ(service()->weak_credential_count(), 1UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedStoreTest,
       DetectChangingLeakedPassword) {
  password_store().AddLogin(MakeForm(kUsername2, kPassword, kOrigin1, true));
  RunUntilIdle();
  EXPECT_EQ(service()->compromised_credential_count(), 1UL);

  // When a leaked password is changed it is no longer leaked.
  password_store().UpdateLogin(MakeForm(kUsername2, kPassword2, kOrigin1));
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
  password_store().AddLogin(MakeForm(kUsername3, kPassword, kOrigin1));
  password_store().AddLogin(MakeForm(kUsername4, kPassword, kOrigin2));
  AdvanceClockForWeakAndReusedChecks();
  EXPECT_EQ(service()->reused_credential_count(), 2UL);

  // After changing one the reused passwords, there are now 0.
  password_store().UpdateLogin(MakeForm(kUsername3, kPassword2, kOrigin1));
  AdvanceClockForWeakAndReusedChecks();
  EXPECT_EQ(service()->reused_credential_count(), 0UL);

  // Changing a password to be the same as an existing one should be picked up.
  password_store().UpdateLogin(MakeForm(kUsername4, kPassword2, kOrigin2));
  AdvanceClockForWeakAndReusedChecks();
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

// TODO: sideyilmaz@chromium.org - Investigate why this test fails.
TEST_F(PasswordStatusCheckServiceBaseTest,
       DISABLED_PasswordCheckSignedOutWithPasswords) {
  profile_store().AddLogin(MakeForm(kUsername1, kPassword, kOrigin1));

  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kSignedOut));
  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest,
       DISABLED_PasswordCheck_FindCompromised) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Store credential that has no issue associated with it.
  profile_store().AddLogin(MakeForm(kUsername1, kPassword, kOrigin1));
  RunUntilIdle();

  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  // When leak check runs, mock the result for this credential coming back
  // positive.
  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  RunUntilIdle();

  // Enumlate response from leak check server.
  static_cast<BulkLeakCheckDelegateInterface*>(bulk_leak_check_service())
      ->OnFinishedCredential(LeakCheckCredential(kUsername1, kPassword),
                             IsLeaked(true));

  bulk_leak_check_service()->set_state_and_notify(
      BulkLeakCheckService::State::kIdle);
  RunUntilIdle();

  // New leak is now picked up by service.
  EXPECT_EQ(service()->compromised_credential_count(), 1UL);
}

TEST_F(PasswordStatusCheckServiceBaseTest, DISABLED_PasswordCheck_Error) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  profile_store().AddLogin(MakeForm(kUsername1, kPassword, kOrigin1));

  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  task_environment()->AdvanceClock(
      service()->GetScheduledPasswordCheckInterval());
  RunUntilIdle();

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

  // Set scheduled time to be a bit in the past (less than interval).
  service()->SetPasswordCheckSchedulePrefsWithInterval(
      base::Time::Now() - 0.5 * service()->GetScheduledPasswordCheckInterval());

  base::TimeDelta interval_before =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_before = service()->GetScheduledPasswordCheckTime();

  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));

  service()->StartRepeatedUpdates();

  // If the scheduled check time is in the past, it should run within the
  // overdue interval.
  task_environment()->AdvanceClock(
      features::kPasswordCheckOverdueInterval.Get());
  RunUntilIdle();

  // After password check is completed, the next one should be scheduled.
  base::TimeDelta interval_after =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_after = service()->GetScheduledPasswordCheckTime();

  ASSERT_EQ(interval_before, interval_after);
  ASSERT_EQ(check_time_before + interval_before, check_time_after);
  ASSERT_GT(check_time_after, base::Time::Now());
}

TEST_F(PasswordStatusCheckServiceBaseTest,
       CheckTimeUpdatedAfterRunScheduledLongTimeInThePast) {
  // Set scheduled time to be a long time (more than interval) in the past.
  service()->SetPasswordCheckSchedulePrefsWithInterval(
      base::Time::Now() - 5 * service()->GetScheduledPasswordCheckInterval());
  base::TimeDelta interval_before =
      service()->GetScheduledPasswordCheckInterval();

  service()->StartRepeatedUpdates();

  // If the scheduled check time is in the past, it should run within the
  // overdue interval.
  task_environment()->AdvanceClock(
      features::kPasswordCheckOverdueInterval.Get());
  RunUntilIdle();

  // After password check is completed, the next one should be scheduled.
  base::TimeDelta interval_after =
      service()->GetScheduledPasswordCheckInterval();
  base::Time check_time_after = service()->GetScheduledPasswordCheckTime();

  ASSERT_EQ(interval_before, interval_after);
  ASSERT_GT(check_time_after, base::Time::Now());
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

TEST_F(PasswordStatusCheckServiceBaseTest, WeakAndReusesCheckRunningDaily) {
  // Check that there is no weak or reused passwords.
  ASSERT_EQ(0UL, service()->weak_credential_count());
  ASSERT_EQ(0UL, service()->reused_credential_count());

  // Add a weak password.
  profile_store().AddLogin(WeakForm());

  // Weak passwords should not be flagged yet, as the weak check did not run.
  ASSERT_EQ(0UL, service()->weak_credential_count());
  ASSERT_EQ(0UL, service()->reused_credential_count());

  AdvanceClockForWeakAndReusedChecks();
  // Weak password should be detected.
  ASSERT_EQ(1UL, service()->weak_credential_count());
  ASSERT_EQ(0UL, service()->reused_credential_count());

  // Add two reused passwords.
  profile_store().AddLogin(ReusedForm1());
  profile_store().AddLogin(ReusedForm2());

  AdvanceClockForWeakAndReusedChecks();
  // Reused passwords should be detected.
  ASSERT_EQ(1UL, service()->weak_credential_count());
  ASSERT_EQ(2UL, service()->reused_credential_count());
}

TEST_F(PasswordStatusCheckServiceBaseTest, IgnoredCompromisedPasswords) {
  // Check that there is no compromised passwords.
  ASSERT_EQ(0UL, service()->compromised_credential_count());

  // Check a compromised password is detected.
  profile_store().AddLogin(MakeForm(kUsername1, kPassword, kOrigin1, true));
  AdvanceClockForWeakAndReusedChecks();
  ASSERT_EQ(1UL, service()->compromised_credential_count());

  // Check leaked but muted password is not detected.
  PasswordForm leaked_muted_form = MakeForm(kUsername1, kPassword, kOrigin2);
  leaked_muted_form.password_issues.insert_or_assign(
      password_manager::InsecureType::kLeaked,
      password_manager::InsecurityMetadata(
          base::Time::Now(), password_manager::IsMuted(true),
          password_manager::TriggerBackendNotification(false)));
  profile_store().AddLogin(leaked_muted_form);
  AdvanceClockForWeakAndReusedChecks();
  ASSERT_EQ(1UL, service()->compromised_credential_count());

  // Check phished but muted password is not detected.
  PasswordForm phished_muted_form = MakeForm(kUsername2, kPassword, kOrigin2);
  phished_muted_form.password_issues.insert_or_assign(
      password_manager::InsecureType::kPhished,
      password_manager::InsecurityMetadata(
          base::Time::Now(), password_manager::IsMuted(true),
          password_manager::TriggerBackendNotification(false)));
  profile_store().AddLogin(phished_muted_form);
  AdvanceClockForWeakAndReusedChecks();
  ASSERT_EQ(1UL, service()->compromised_credential_count());
}

TEST_F(PasswordStatusCheckServiceBaseTest, IgnoredSavedPasswords) {
  // Check that there is no weak or reused passwords.
  ASSERT_EQ(0UL, service()->weak_credential_count());

  // Add a weak password.
  profile_store().AddLogin(MakeForm(kUsername1, kWeakPassword, kOrigin1));
  AdvanceClockForWeakAndReusedChecks();
  ASSERT_EQ(1UL, service()->weak_credential_count());

  // Add a federated weak password, this should be ignored.
  PasswordForm federated_from = MakeForm(kUsername2, kWeakPassword, kOrigin1);
  federated_from.federation_origin =
      url::SchemeHostPort(GURL("https://idp.com"));
  federated_from.password_value.clear();
  federated_from.signon_realm = "federation://example.com/accounts.com";
  federated_from.match_type = PasswordForm::MatchType::kExact;
  profile_store().AddLogin(federated_from);
  AdvanceClockForWeakAndReusedChecks();
  ASSERT_EQ(1UL, service()->weak_credential_count());
}

TEST_P(PasswordStatusCheckServiceParameterizedCardTest, PasswordCardState) {
  // Setup test based on parameters.
  profile().GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableService,
      password_saving_allowed());
  if (include_safe_password()) {
    profile_store().AddLogin(MakeForm(kUsername1, kPassword, kOrigin1));
  }
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
  AdvanceClockForWeakAndReusedChecks();
  if (check_ran_previously()) {
    profile().GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().InSecondsFSinceUnixEpoch());
  }

  size_t weak_count = service()->weak_credential_count();
  size_t compromised_count = service()->compromised_credential_count();
  size_t reused_count = service()->reused_credential_count();

  base::Value::Dict card = service()->GetPasswordCardData(signed_in());

  std::u16string header =
      base::UTF8ToUTF16(*card.FindString(safety_hub::kCardHeaderKey));
  std::u16string subheader =
      base::UTF8ToUTF16(*card.FindString(safety_hub::kCardSubheaderKey));
  int state = card.FindInt(safety_hub::kCardStateKey).value();

  // User doesn't have passwords.
  if (!any_password_saved() && password_saving_allowed()) {
    EXPECT_EQ(header,
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_HEADER_NO_PASSWORDS));
    EXPECT_EQ(
        subheader,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_NO_PASSWORDS));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kInfo));
    return;
  }

  // Saving passwords disabled due to enterprise policy.
  if (!any_password_saved() && !password_saving_allowed()) {
    EXPECT_EQ(header,
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_HEADER_NO_PASSWORDS));
    EXPECT_EQ(
        subheader,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_NO_PASSWORDS_POLICY));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kInfo));
    return;
  }

  CHECK(any_password_saved());

  // Compromised passwords show a warning regardless of sign-in status.
  if (compromised_count > 0) {
    EXPECT_EQ(header, l10n_util::GetPluralStringFUTF16(
                          IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_COUNT,
                          compromised_count));

    // Check the subheader string (singular version).
    EXPECT_EQ(subheader,
              l10n_util::GetPluralStringFUTF16(
                  IDS_PASSWORD_MANAGER_UI_HAS_COMPROMISED_PASSWORDS, 1));
    // Check the subheader string (plural version) by adding one more
    // compromised password.
    profile_store().AddLogin(MakeForm(kUsername3, kPassword, kOrigin2, true));
    RunUntilIdle();
    card = service()->GetPasswordCardData(signed_in());
    subheader =
        base::UTF8ToUTF16(*card.FindString(safety_hub::kCardSubheaderKey));
    EXPECT_EQ(subheader,
              l10n_util::GetPluralStringFUTF16(
                  IDS_PASSWORD_MANAGER_UI_HAS_COMPROMISED_PASSWORDS, 2));

    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kWarning));
    return;
  }

  if (reused_count > 0 && signed_in()) {
    EXPECT_EQ(header, l10n_util::GetPluralStringFUTF16(
                          IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_COUNT,
                          reused_count));
    EXPECT_EQ(subheader, l10n_util::GetStringUTF16(
                             IDS_PASSWORD_MANAGER_UI_HAS_REUSED_PASSWORDS));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kWeak));
    return;
  }

  if (weak_count > 0 && signed_in()) {
    EXPECT_EQ(header,
              l10n_util::GetPluralStringFUTF16(
                  IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_COUNT, weak_count));
    EXPECT_EQ(subheader, l10n_util::GetStringUTF16(
                             IDS_PASSWORD_MANAGER_UI_HAS_WEAK_PASSWORDS));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kWeak));
    return;
  }

  if (!any_issue_included() && check_ran_previously() && signed_in()) {
    EXPECT_EQ(header,
              l10n_util::GetPluralStringFUTF16(
                  IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_COUNT, 0));
    EXPECT_EQ(subheader,
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_RECENTLY));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kSafe));
    return;
  }

  if (!any_issue_included() && !check_ran_previously() && signed_in()) {
    EXPECT_EQ(
        header,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_HEADER_NO_WEAK_OR_REUSED));
    EXPECT_EQ(
        subheader,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_GO_TO_PASSWORD_MANAGER));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kInfo));
    return;
  }

  if (reused_count > 0 && !signed_in()) {
    EXPECT_EQ(header, l10n_util::GetPluralStringFUTF16(
                          IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_COUNT,
                          reused_count));
    EXPECT_EQ(subheader,
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SIGN_IN));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kWeak));
    return;
  }

  if (weak_count > 0 && !signed_in()) {
    EXPECT_EQ(header,
              l10n_util::GetPluralStringFUTF16(
                  IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_COUNT, weak_count));
    EXPECT_EQ(subheader,
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SIGN_IN));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kWeak));
    return;
  }

  if (!any_issue_included() && !signed_in()) {
    EXPECT_EQ(
        header,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_HEADER_NO_WEAK_OR_REUSED));
    EXPECT_EQ(subheader,
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SIGN_IN));
    EXPECT_EQ(state, static_cast<int>(SafetyHubCardState::kInfo));
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

TEST_F(PasswordStatusCheckServiceBaseTest, PasswordCardCheckTime) {
  // Add a password without issues to reach safe state.
  profile_store().AddLogin(MakeForm(kUsername1, kPassword, kOrigin1));
  RunUntilIdle();

  SetLastCheckTime(base::TimeDelta(base::Seconds(0)));
  EXPECT_EQ(*service()->GetPasswordCardData(true).FindString(
                safety_hub::kCardSubheaderKey),
            std::string("Checked just now"));

  SetLastCheckTime(base::TimeDelta(base::Seconds(3)));
  EXPECT_EQ(*service()->GetPasswordCardData(true).FindString(
                safety_hub::kCardSubheaderKey),
            std::string("Checked just now"));

  SetLastCheckTime(base::TimeDelta(base::Minutes(3)));
  EXPECT_EQ(*service()->GetPasswordCardData(true).FindString(
                safety_hub::kCardSubheaderKey),
            std::string("Checked 3 minutes ago"));

  SetLastCheckTime(base::TimeDelta(base::Hours(3)));
  EXPECT_EQ(*service()->GetPasswordCardData(true).FindString(
                safety_hub::kCardSubheaderKey),
            std::string("Checked 3 hours ago"));

  SetLastCheckTime(base::TimeDelta(base::Days(3)));
  EXPECT_EQ(*service()->GetPasswordCardData(true).FindString(
                safety_hub::kCardSubheaderKey),
            std::string("Checked 3 days ago"));

  SetLastCheckTime(base::TimeDelta(base::Days(300)));
  EXPECT_EQ(*service()->GetPasswordCardData(true).FindString(
                safety_hub::kCardSubheaderKey),
            std::string("Checked 300 days ago"));
}

TEST_P(PasswordStatusCheckServiceParameterizedStoreTest,
       ResultWhenChangingLeakedPassword) {
  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_old_result =
      service()->GetCachedResult();
  EXPECT_TRUE(opt_old_result.has_value());
  PasswordStatusCheckResult* old_result =
      static_cast<PasswordStatusCheckResult*>(opt_old_result.value().get());
  EXPECT_THAT(old_result->GetCompromisedPasswords(), testing::IsEmpty());

  // When a leaked password is found, the result should be updated.
  password_store().AddLogin(MakeForm(kUsername2, kPassword, kOrigin1, true));
  RunUntilIdle();

  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_new_result =
      service()->GetCachedResult();
  EXPECT_TRUE(opt_new_result.has_value());
  PasswordStatusCheckResult* new_result =
      static_cast<PasswordStatusCheckResult*>(opt_new_result.value().get());
  EXPECT_THAT(new_result->GetCompromisedPasswords(),
              testing::ElementsAre(
                  PasswordPair(kOrigin1, base::UTF16ToASCII(kUsername2))));
}

TEST_P(PasswordStatusCheckServiceParameterizedSchedulingTest,
       CheckWeightedRandomScheduling) {
  base::test::ScopedFeatureList feature_list;
  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  // Make the probabality of all other days 0, except the current week day.
  // Current week day should be selected to run the checks.
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSafetyHub,
      {
          {"password-check-sun-weight", GetWeightForDay(0)},
          {"password-check-mon-weight", GetWeightForDay(1)},
          {"password-check-tue-weight", GetWeightForDay(2)},
          {"password-check-wed-weight", GetWeightForDay(3)},
          {"password-check-thu-weight", GetWeightForDay(4)},
          {"password-check-fri-weight", GetWeightForDay(5)},
          {"password-check-sat-weight", GetWeightForDay(6)},
      });

  // The first run is scheduled as soon as the service created on set up.
  // Call StartRepeatedUpdates to let the first run schedued. Any following
  // checks should be scheduled for the current weekday.
  service()->StartRepeatedUpdates();

  // Check the next scheduled run is on GetCurrentWeekday.
  base::Time next_check_time = service()->GetScheduledPasswordCheckTime();
  base::Time::Exploded next_check_time_exploded;
  next_check_time.LocalExplode(&next_check_time_exploded);
  EXPECT_EQ(next_check_time_exploded.day_of_week,
            /* day_of_week */ GetCurrentWeekday());
  feature_list.Reset();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordStatusCheckServiceParameterizedCardTest,
    ::testing::Combine(/*include_compromised*/ ::testing::Bool(),
                       /*include_weak*/ ::testing::Bool(),
                       /*include_reused*/ ::testing::Bool(),
                       /*check_ran_previously*/ ::testing::Bool(),
                       /*signed_in*/ ::testing::Bool(),
                       /*include_safe_password*/ ::testing::Bool(),
                       /*password_saving_allowed*/ ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordStatusCheckServiceParameterizedIssueTest,
    ::testing::Combine(/*include_weak*/ ::testing::Bool(),
                       /*include_compromised*/ ::testing::Bool(),
                       /*include_reused*/ ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(AccountOrProfileStore,
                         PasswordStatusCheckServiceParameterizedStoreTest,
                         testing::Bool());

// A range from 0 (inclusive) to 7 (exclusive) to test randomization for each
// day of week.
INSTANTIATE_TEST_SUITE_P(All,
                         PasswordStatusCheckServiceParameterizedSchedulingTest,
                         testing::Range(0, 7));
