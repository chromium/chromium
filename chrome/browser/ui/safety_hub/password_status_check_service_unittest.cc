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
constexpr char kOrigin[] = "https://example.com/";
constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";
constexpr char16_t kUsername3[] = u"charlie";
constexpr char16_t kPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";

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

PasswordForm MakeCredential(base::StringPiece16 username,
                            base::StringPiece16 password) {
  PasswordForm form;
  form.username_value = username;
  form.password_value = password;
  form.signon_realm = kOrigin;
  form.url = GURL(kOrigin);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

PasswordForm MakeInsecureCredential(InsecureType type) {
  // Use different usernames for different issue types so credentials with these
  // issues can be stored in parallel.
  base::StringPiece16 username;
  switch (type) {
    case InsecureType::kWeak:
      username = kUsername1;
      break;
    case InsecureType::kReused:
      username = kUsername2;
      break;
    case InsecureType::kPhished:
    case InsecureType::kLeaked:
      username = kUsername3;
      break;
  }
  PasswordForm form = MakeCredential(username, kPassword);
  form.password_issues.insert_or_assign(
      type, password_manager::InsecurityMetadata(
                base::Time::Now(), password_manager::IsMuted(false),
                password_manager::TriggerBackendNotification(false)));
  return form;
}

}  // namespace

class PasswordStatusCheckServiceBaseTest : public testing::Test {
 public:
  void CreateService() {
    service_ = std::make_unique<PasswordStatusCheckService>(&profile_);
    RunUntilIdle();
  }

  void StoreCredentialWithIssue(InsecureType type) {
    PasswordForm form = MakeInsecureCredential(type);
    password_store_->AddLogin(form);
  }

  void StoreCredential(base::StringPiece16 username,
                       base::StringPiece16 password) {
    PasswordForm form = MakeCredential(username, password);
    password_store_->AddLogin(form);
  }

  void UpdateInsecureCredentials() {
    service()->UpdateInsecureCredentialCountAsync();
    RunUntilIdle();
  }

  TestingProfile& profile() { return profile_; }
  PasswordStatusCheckService* service() { return service_.get(); }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  raw_ptr<BulkLeakCheckService> bulk_leak_check_service() {
    return bulk_leak_check_service_;
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

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

  content::BrowserTaskEnvironment task_env_;

  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;

  scoped_refptr<TestPasswordStore> password_store_ =
      CreateAndUseTestPasswordStore(&profile_);

  raw_ptr<BulkLeakCheckService> bulk_leak_check_service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);

  std::unique_ptr<PasswordStatusCheckService> service_;
};

class PasswordStatusCheckServiceParameterizedTest
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

TEST_F(PasswordStatusCheckServiceBaseTest, NoIssuesInitially) {
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->weak_credential_count(), 0UL);
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
  EXPECT_EQ(service()->reused_credential_count(), 0UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedTest,
       GetPreexistingMultipleIssueCounts) {
  // Based on test parameters, add different credential issues to the store.
  if (include_weak()) {
    StoreCredentialWithIssue(InsecureType::kWeak);
  }
  if (include_compromised()) {
    StoreCredentialWithIssue(InsecureType::kLeaked);
  }
  if (include_reused()) {
    StoreCredentialWithIssue(InsecureType::kReused);
  }

  // Service is restarted and existing issues are found during construction.
  CreateService();
  EXPECT_EQ(service()->weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service()->compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service()->reused_credential_count(), include_reused() ? 1UL : 0UL);
}

TEST_P(PasswordStatusCheckServiceParameterizedTest, GetMultipleIssueCounts) {
  // Based on test parameters, add different credential issues to the store.
  if (include_weak()) {
    StoreCredentialWithIssue(InsecureType::kWeak);
  }
  if (include_compromised()) {
    StoreCredentialWithIssue(InsecureType::kLeaked);
  }
  if (include_reused()) {
    StoreCredentialWithIssue(InsecureType::kReused);
  }

  // Expect to find credential issues that were added while service is active.
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service()->compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service()->reused_credential_count(), include_reused() ? 1UL : 0UL);
}

TEST_F(PasswordStatusCheckServiceBaseTest, RepeatedlyUpdatingDoesNotCrash) {
  for (int i = 0; i < 5; ++i) {
    service()->UpdateInsecureCredentialCountAsync();
    service()->RunPasswordCheckAsync();
  }
  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest, PasswordCheckNoPasswords) {
  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  service()->RunPasswordCheckAsync();
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(observer, OnStateChanged(BulkLeakCheckService::State::kIdle));

  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest,
       PasswordCheckSignedOutWithPasswords) {
  StoreCredential(kUsername1, kPassword);

  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  service()->RunPasswordCheckAsync();
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kSignedOut));
  RunUntilIdle();
}

TEST_F(PasswordStatusCheckServiceBaseTest, PasswordCheck_FindCompromised) {
  identity_test_env().MakeAccountAvailable(kTestEmail);

  // Store credential that has no issue associated with it.
  StoreCredential(kUsername1, kPassword);
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  // When leak check runs, mock the result for this credential coming back
  // positive.
  service()->RunPasswordCheckAsync();
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

  StoreCredential(kUsername1, kPassword);
  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);

  ::testing::StrictMock<MockObserver> observer(bulk_leak_check_service());

  service()->RunPasswordCheckAsync();
  EXPECT_CALL(observer,
              OnStateChanged(BulkLeakCheckService::State::kServiceError));

  bulk_leak_check_service()->set_state_and_notify(
      BulkLeakCheckService::State::kServiceError);

  RunUntilIdle();

  UpdateInsecureCredentials();
  EXPECT_EQ(service()->compromised_credential_count(), 0UL);
}

TEST_F(PasswordStatusCheckServiceBaseTest, PrefInitialized) {
  ASSERT_TRUE(profile().GetPrefs()->HasPrefPath(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval));
  const base::Value::Dict& check_schedule_dict = profile().GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);

  absl::optional<base::TimeDelta> interval_used_for_scheduling =
      base::ValueToTimeDelta(check_schedule_dict.Find(
          safety_hub_prefs::kPasswordCheckIntervalKey));
  ASSERT_TRUE(interval_used_for_scheduling.has_value());
  ASSERT_EQ(interval_used_for_scheduling.value(),
            features::kBackgroundPasswordCheckInterval.Get());

  absl::optional<base::Time> check_time = base::ValueToTime(
      check_schedule_dict.Find(safety_hub_prefs::kNextPasswordCheckTimeKey));
  ASSERT_TRUE(check_time.has_value());
  ASSERT_GE(check_time.value(), base::Time::Now());
  ASSERT_LT(check_time.value(),
            base::Time::Now() + interval_used_for_scheduling.value());
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

  const base::Value::Dict& dict_before = profile().GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);
  absl::optional<base::TimeDelta> interval_before = base::ValueToTimeDelta(
      dict_before.Find(safety_hub_prefs::kPasswordCheckIntervalKey));
  absl::optional<base::Time> check_time_before = base::ValueToTime(
      dict_before.Find(safety_hub_prefs::kNextPasswordCheckTimeKey));

  base::FieldTrialParams params_after;
  params_after[features::kBackgroundPasswordCheckInterval.name] = "20d";
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(features::kSafetyHub,
                                                  params_after);

  service()->StartRepeatedUpdates();

  const base::Value::Dict& dict_after = profile().GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);
  absl::optional<base::TimeDelta> interval_after = base::ValueToTimeDelta(
      dict_after.Find(safety_hub_prefs::kPasswordCheckIntervalKey));
  absl::optional<base::Time> check_time_after = base::ValueToTime(
      dict_after.Find(safety_hub_prefs::kNextPasswordCheckTimeKey));

  ASSERT_EQ(interval_before.value() * 2, interval_after.value());
  ASSERT_NE(check_time_before.value(), check_time_after.value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordStatusCheckServiceParameterizedTest,
    ::testing::Combine(/*include_weak*/ ::testing::Bool(),
                       /*include_compromised*/ ::testing::Bool(),
                       /*include_reused*/ ::testing::Bool()));
