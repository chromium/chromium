// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

const char kProfileId[] = "dummy@gmail.com";
const char kRemoteUrl[] = "www.example.com";

class MockDelegate : public AnnouncementNotificationService::Delegate {
 public:
  MockDelegate() = default;

  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;

  ~MockDelegate() override = default;
  MOCK_METHOD0(ShowNotification, void());
  MOCK_METHOD0(IsFirstRun, bool());
};

class AnnouncementNotificationServiceTest : public testing::Test {
 public:
  AnnouncementNotificationServiceTest() = default;

  AnnouncementNotificationServiceTest(
      const AnnouncementNotificationServiceTest&) = delete;
  AnnouncementNotificationServiceTest& operator=(
      const AnnouncementNotificationServiceTest&) = delete;

  ~AnnouncementNotificationServiceTest() override = default;

 protected:
  AnnouncementNotificationService* service() {
    DCHECK(service_) << "Call Init() first.";
    return service_.get();
  }

  MockDelegate* delegate() { return delegate_; }

  base::SimpleTestClock* clock() { return &clock_; }

  int CurrentVersionPref() const {
    return pref_service_->GetInteger(kCurrentVersionPrefName);
  }

  base::Time FirstRunTimePref() const {
    return pref_service_->GetTime(kAnnouncementFirstRunTimePrefName);
  }

  base::Time SetFirstRunTimePref(const char* time_str) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_str, &time));
    pref_service_->SetTime(kAnnouncementFirstRunTimePrefName, time);
    return time;
  }

  // Sets the current time used in test. Assume UTC time when time zone is not
  // specified.
  base::Time SetNow(const char* now_str) {
    base::Time now;
    EXPECT_TRUE(base::Time::FromUTCString(now_str, &now));
    clock()->SetNow(now);
    EXPECT_FALSE(now.is_null());
    return now;
  }

  void Init(const std::map<std::string, std::string>& parameters,
            bool enable_feature,
            bool sign_in,
            int current_version,
            bool new_profile,
            bool guest_profile = false) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (enable_feature)
      enabled_features.emplace_back(kAnnouncementNotification, parameters);
    else
      disabled_features.push_back(kAnnouncementNotification);

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);

    // Setup sign in status.
    test_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(test_profile_manager_->SetUp());

    // Build the testing profile.
    TestingProfile::Builder builder;
    builder.SetPath(
        test_profile_manager_->profiles_dir().AppendASCII(kProfileId));
    builder.SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable>());
    builder.SetProfileName(kProfileId);
    builder.SetIsNewProfile(new_profile);
    if (guest_profile)
      builder.SetGuestSession();
    test_profile_ = builder.Build();

    // Mock the sign in profile data.
    DCHECK_EQ(test_profile_->GetPath(),
              test_profile_manager_->profiles_dir().AppendASCII(kProfileId));
    ProfileAttributesInitParams params;
    params.profile_path =
        test_profile_manager_->profiles_dir().AppendASCII(kProfileId);
    params.profile_name = u"dummy_name";
    params.gaia_id = sign_in ? "dummy_gaia_id" : std::string();
    params.is_consented_primary_account = sign_in;
    test_profile_manager_->profile_attributes_storage()->AddProfile(
        std::move(params));

    // Register pref.
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AnnouncementNotificationService::RegisterProfilePrefs(
        pref_service_->registry());
    pref_service_->SetInteger(kCurrentVersionPrefName, current_version);

    // Setup test target objects.
    auto delegate = std::make_unique<NiceMock<MockDelegate>>();
    delegate_ = delegate.get();
    service_ = AnnouncementNotificationService::Create(
        test_profile_.get(), pref_service_.get(), std::move(delegate), &clock_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> test_profile_manager_;
  base::SimpleTestClock clock_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AnnouncementNotificationService> service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingProfile> test_profile_;
  raw_ptr<MockDelegate> delegate_ = nullptr;
};

TEST_F(AnnouncementNotificationServiceTest, RequireSignOut) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"}, {kVersion, "2"}, {kRequireSignout, "true"}};
  // Profile now is signed in.
  Init(parameters, true, true /*sign_in*/, 1, false);

  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification()).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
}

TEST_F(AnnouncementNotificationServiceTest, SkipNewProfile) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"}, {kVersion, "2"}, {kSkipNewProfile, "true"}};
  Init(parameters, true, false, 1, true /*new_profile*/);

  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification()).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
}

TEST_F(AnnouncementNotificationServiceTest, SkipGuestProfile) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"}, {kVersion, "2"}, {kSkipNewProfile, "false"}};
  Init(parameters, true, false, 1, false, /*guest_profile=*/true);
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification()).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
}

TEST_F(AnnouncementNotificationServiceTest, RemoteUrl) {
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"},
      {kVersion, "4"},
      {kSkipNewProfile, "true"},
      {kAnnouncementUrl, kRemoteUrl}};
  Init(parameters, true, false, 1, false);
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification());
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 4);
}

// First run timestamp should be persisted even if the Finch parameter is not
// received or the feature is disabled.
TEST_F(AnnouncementNotificationServiceTest, SaveFirstRunTimeOnFirstRun) {
  base::Time now = SetNow("30 May 2018 12:00:00");
  std::map<std::string, std::string> parameters;
  Init(parameters, false /*enable_feature*/, false, -1, true);
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(true));
  EXPECT_CALL(*delegate(), ShowNotification()).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), -1);
  EXPECT_EQ(FirstRunTimePref(), now);
}

// First run timestamp should not be persisted when not on first run.
TEST_F(AnnouncementNotificationServiceTest, SaveFirstRunTimeNotFirstRun) {
  SetNow("30 May 2018 12:00:00");
  std::map<std::string, std::string> parameters;
  Init(parameters, false /*enable_feature*/, false, -1, true);
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), -1);
  EXPECT_EQ(FirstRunTimePref(), base::Time());
}

// Not to show notification if first run timestamp happens after Finch parameter
// timestamp.
TEST_F(AnnouncementNotificationServiceTest, SkipFirstRunAfterTimeNotShow) {
  SetNow("10 Feb 2020 13:00:00 GMT");
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"},
      {kVersion, "2"},
      {kSkipNewProfile, "false"},
      {kSkipFirstRunAfterTime, "10 Feb 2020 12:15:00 GMT"}};
  Init(parameters, true, false, 1, false);
  auto first_run_time = SetFirstRunTimePref("10 Feb 2020 12:30:00 GMT");
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification()).Times(0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
  EXPECT_EQ(FirstRunTimePref(), first_run_time);
}

// Show notification if first run timestamp happens before Finch parameter
// timestamp.
TEST_F(AnnouncementNotificationServiceTest, SkipFirstRunAfterTimeShow) {
  SetNow("10 Feb 2020 13:00:00 GMT");
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"},
      {kVersion, "2"},
      {kSkipNewProfile, "false"},
      {kSkipFirstRunAfterTime, "10 Feb 2020 12:15:00 GMT"}};
  Init(parameters, true, false, 1, false);
  SetFirstRunTimePref("10 Feb 2020 12:10:00 GMT");
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification());
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
}

// Show notification if there is no first run timestamp but Finch has
// "skip_first_run_after_time" parameter.
TEST_F(AnnouncementNotificationServiceTest, SkipFirstRunAfterNoFirstRunPref) {
  SetNow("10 Feb 2020 13:00:00 GMT");
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"},
      {kVersion, "2"},
      {kSkipNewProfile, "false"},
      {kSkipFirstRunAfterTime, "10 Feb 2020 12:15:00 GMT"}};
  Init(parameters, true, false, 1, false);
  EXPECT_EQ(FirstRunTimePref(), base::Time());
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification());
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), 2);
  EXPECT_EQ(FirstRunTimePref(), base::Time());
}

// "skip_first_run_after_time" parameter should assume UTC when time zone is not
// specified.
TEST_F(AnnouncementNotificationServiceTest, SkipFirstRunAfterAssumeUTCTime) {
  SetNow("10 Feb 2020 13:00:00 GMT");
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, "false"},
      {kVersion, "2"},
      {kSkipNewProfile, "false"},
      // No time zone specified. The time string should assume UTC.
      {kSkipFirstRunAfterTime, "10 Feb 2020 12:59:59"}};
  Init(parameters, true, false, 1, false);
  EXPECT_EQ(FirstRunTimePref(), base::Time());
  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(false));
  EXPECT_CALL(*delegate(), ShowNotification());
  service()->MaybeShowNotification();
}

struct VersionTestParam {
  bool enable_feature;
  bool skip_first_run;
  bool is_first_run;
  int version;
  int current_version;
  bool show_notification_called;
  int expected_version_pref;
};

class AnnouncementNotificationServiceVersionTest
    : public AnnouncementNotificationServiceTest,
      public ::testing::WithParamInterface<VersionTestParam> {
 public:
  AnnouncementNotificationServiceVersionTest() = default;

  AnnouncementNotificationServiceVersionTest(
      const AnnouncementNotificationServiceVersionTest&) = delete;
  AnnouncementNotificationServiceVersionTest& operator=(
      const AnnouncementNotificationServiceVersionTest&) = delete;

  ~AnnouncementNotificationServiceVersionTest() override = default;
};

const VersionTestParam kVersionTestParams[] = {
    // First run. No current version in pref, has Finch parameter.
    {true, false /*skip_first_run*/, true /*is_first_run*/, 1, -1, true, 1},
    // Skip first run.
    {true, true /*skip_first_run*/, true /*is_first_run*/, 2, -1, false, 2},
    {true, true /*skip_first_run*/, false /*is_first_run*/, 2, -1, true, 2},
    // DisableFeature
    {false /*enable_feature*/, false, false, 1, -1, false, -1},
    // Same version between Finch parameter and preference.
    {true, false, false, 3 /*version*/, 3 /*current_version*/, false, 3},
    // New version from Finch parameter.
    {true, false, false, 4 /*version*/, 3 /*current_version*/, true, 4},
    // OldVersion
    {true, false, false, 2 /*version*/, 3 /*current_version*/, false, 2},
    // No current version in pref, no Finch parameter.
    {true, false, false, -1 /*version*/, -1 /*current_version*/, false, -1},
    // Has current version in pref, no Finch parameter.
    {true, false, false, -1 /*version*/, 10 /*current_version*/, false, 10},
};

TEST_P(AnnouncementNotificationServiceVersionTest, VersionTest) {
  const auto& param = GetParam();
  auto now = SetNow("10 Feb 2020 13:00:00");
  std::map<std::string, std::string> parameters = {
      {kSkipFirstRun, param.skip_first_run ? "true" : "false"},
      {kVersion, base::NumberToString(param.version)}};
  Init(parameters, param.enable_feature, false /*sign_in*/,
       param.current_version, false);

  ON_CALL(*delegate(), IsFirstRun()).WillByDefault(Return(param.is_first_run));
  EXPECT_CALL(*delegate(), ShowNotification())
      .Times(param.show_notification_called ? 1 : 0);
  service()->MaybeShowNotification();
  EXPECT_EQ(CurrentVersionPref(), param.expected_version_pref);
  EXPECT_EQ(FirstRunTimePref(), param.is_first_run ? now : base::Time());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AnnouncementNotificationServiceVersionTest,
                         testing::ValuesIn(kVersionTestParams));

}  // namespace
