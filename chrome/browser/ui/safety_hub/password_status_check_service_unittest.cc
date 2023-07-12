// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service.h"

#include <string>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kOrigin[] = "https://example.com/";
constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";
constexpr char16_t kUsername3[] = u"charlie";
constexpr char16_t kPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";

using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

PasswordForm MakeInsecurePassword(InsecureType type) {
  PasswordForm form;
  // Use different usernames for different issue types so credentials with these
  // issues can be stored in parallel.
  switch (type) {
    case InsecureType::kWeak:
      form.username_value = kUsername1;
      break;
    case InsecureType::kReused:
      form.username_value = kUsername2;
      break;
    case InsecureType::kPhished:
    case InsecureType::kLeaked:
      form.username_value = kUsername3;
      break;
  }
  form.signon_realm = kOrigin;
  form.url = GURL(kOrigin);
  form.password_value = kPassword;
  form.in_store = PasswordForm::Store::kProfileStore;
  form.password_issues.insert_or_assign(
      type, password_manager::InsecurityMetadata(
                base::Time::Now(), password_manager::IsMuted(false),
                password_manager::TriggerBackendNotification(false)));
  return form;
}

}  // namespace

class PasswordStatusCheckServiceTest
    : public testing::TestWithParam<
          ::testing::tuple</*include_weak*/ bool,
                           /*include_compromised*/ bool,
                           /*include_reused*/ bool>> {
 public:
  void StorePasswordWithIssue(InsecureType type) {
    PasswordForm form = MakeInsecurePassword(type);
    password_store_->AddLogin(form);
  }

  void UpdateInsecureCredentials() {
    base::RunLoop loop;
    service().SetTestingCallback(loop.QuitClosure());
    service().UpdateInsecureCredentialCountAsync();
    ASSERT_TRUE(service().GetSavedPasswordsPresenterForTesting());
    loop.Run();
  }

  TestingProfile& profile() { return profile_; }
  PasswordStatusCheckService& service() { return service_; }

  bool include_weak() const { return std::get<0>(GetParam()); }
  bool include_compromised() const { return std::get<1>(GetParam()); }
  bool include_reused() const { return std::get<2>(GetParam()); }

 private:
  content::BrowserTaskEnvironment task_env_;

  TestingProfile profile_;

  scoped_refptr<TestPasswordStore> password_store_ =
      CreateAndUseTestPasswordStore(&profile_);

  PasswordStatusCheckService service_{&profile_};
};

TEST_P(PasswordStatusCheckServiceTest, GetMultipleIssueCounts) {
  EXPECT_FALSE(service().GetSavedPasswordsPresenterForTesting());

  // Initially, there are no compromised credentials.
  UpdateInsecureCredentials();
  EXPECT_EQ(service().weak_credential_count(), 0UL);
  EXPECT_EQ(service().compromised_credential_count(), 0UL);
  EXPECT_EQ(service().reused_credential_count(), 0UL);

  // Based on test parameters, add different credential issues to the store.
  if (include_weak()) {
    StorePasswordWithIssue(InsecureType::kWeak);
  }
  if (include_compromised()) {
    StorePasswordWithIssue(InsecureType::kLeaked);
  }
  if (include_reused()) {
    StorePasswordWithIssue(InsecureType::kReused);
  }

  // Expect to find credential issues that were added before.
  UpdateInsecureCredentials();
  EXPECT_EQ(service().weak_credential_count(), include_weak() ? 1UL : 0UL);
  EXPECT_EQ(service().compromised_credential_count(),
            include_compromised() ? 1UL : 0UL);
  EXPECT_EQ(service().reused_credential_count(), include_reused() ? 1UL : 0UL);

  EXPECT_FALSE(service().GetSavedPasswordsPresenterForTesting());
}

TEST_F(PasswordStatusCheckServiceTest, RepeatedlyUpdatingDoesNotCrash) {
  base::RunLoop loop;
  service().SetTestingCallback(loop.QuitClosure());
  for (int i = 0; i < 5; ++i) {
    service().UpdateInsecureCredentialCountAsync();
  }
  loop.Run();
  EXPECT_FALSE(service().IsObservingSavedPasswordsPresenterForTesting());
}

TEST_F(PasswordStatusCheckServiceTest, PrefInitialized) {
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
TEST_F(PasswordStatusCheckServiceTest, CheckTimeUpdatedOnIntervalChange) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params_before;
  params_before[features::kBackgroundPasswordCheckInterval.name] = "10d";
  feature_list.InitAndEnableFeatureWithParameters(features::kSafetyHub,
                                                  params_before);

  service().StartRepeatedUpdates();

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

  service().StartRepeatedUpdates();

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
    PasswordStatusCheckServiceTest,
    ::testing::Combine(/*include_weak*/ ::testing::Bool(),
                       /*include_compromised*/ ::testing::Bool(),
                       /*include_reused*/ ::testing::Bool()));
