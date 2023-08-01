// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"

#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"

#endif

namespace {

class MockAppMenuIconControllerDelegate
    : public AppMenuIconController::Delegate {
 public:
  MOCK_METHOD(void,
              UpdateTypeAndSeverity,
              (AppMenuIconController::TypeAndSeverity type_and_severity));
  MOCK_CONST_METHOD1(GetDefaultColorForSeverity,
                     SkColor(AppMenuIconController::Severity severity));
};

// A fake upgrade detector that can broadcast an annoyance level change to its
// observers.
class FakeUpgradeDetector : public UpgradeDetector {
 public:
  FakeUpgradeDetector()
      : UpgradeDetector(base::DefaultClock::GetInstance(),
                        base::DefaultTickClock::GetInstance()) {}

  FakeUpgradeDetector(const FakeUpgradeDetector&) = delete;
  FakeUpgradeDetector& operator=(const FakeUpgradeDetector&) = delete;

  void BroadcastLevel(UpgradeNotificationAnnoyanceLevel level) {
    set_upgrade_notification_stage(level);
    NotifyUpgrade();
  }

  // UpgradeDetector:
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override;
};

base::Time FakeUpgradeDetector::GetAnnoyanceLevelDeadline(
    UpgradeNotificationAnnoyanceLevel level) {
  // This value is not important for this test.
  return base::Time();
}

}  // namespace

bool operator==(const AppMenuIconController::TypeAndSeverity& a,
                const AppMenuIconController::TypeAndSeverity& b) {
  return a.type == b.type && a.severity == b.severity;
}

// A test parameterized on an install mode index. For Google Chrome builds on
// Windows, this allows the test to run for each of the supported side-by-side
// channels. For Chromium builds, there is only the one channel. For non-Win
// builds, there does not appear to be an easy way to run the test as if it were
// a different channel.
class AppMenuIconControllerTest : public ::testing::TestWithParam<int> {
 protected:
  AppMenuIconControllerTest()
#if BUILDFLAG(IS_WIN)
      : install_details_(false, GetParam())
#endif
  {
  }

  AppMenuIconControllerTest(const AppMenuIconControllerTest&) = delete;
  AppMenuIconControllerTest& operator=(const AppMenuIconControllerTest&) =
      delete;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>(&local_state_));
    auto* user_manager = static_cast<user_manager::FakeUserManager*>(
        user_manager::UserManager::Get());
    const auto account_id = AccountId::FromUserEmail("test@test");
    auto* user = user_manager->AddUser(account_id);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
    crosapi::browser_util::RegisterLocalStatePrefs(local_state_.registry());
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TearDown() override { user_manager_.reset(); }
#endif

  UpgradeDetector* upgrade_detector() { return &upgrade_detector_; }
  Profile* profile() { return &profile_; }

  // Returns true if the test is apparently running as an unstable channel.
  bool IsUnstableChannel() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Dev and canary channels are specific to Google Chrome branding.
    return false;
#elif BUILDFLAG(IS_WIN)
    // Windows supports specifying the channel via ScopedInstallDetails.
    return GetParam() >= install_static::DEV_INDEX;
#else
    // Non-Windows platforms don't have a way to specify the channel; see
    // https://crbug.com/903798.
    return false;
#endif
  }

  // Broadcasts a change to |level| to the UpgradeDetector's observers.
  void BroadcastLevel(
      UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
    upgrade_detector_.BroadcastLevel(level);
  }

 private:
#if BUILDFLAG(IS_WIN)
  install_static::ScopedInstallDetails install_details_;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
  TestingPrefServiceSimple local_state_;
#endif

  FakeUpgradeDetector upgrade_detector_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Tests that the controller's delegate is notified with the proper icon type
// and severity when an upgrade is detected.
TEST_P(AppMenuIconControllerTest, UpgradeNotification) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Forcibly enable Lacros Profile migration, so that IDC_LACROS_DATA_MIGRATION
  // becomes visible. Note that profile migration is only enabled if Lacros is
  // the only browser.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({ash::features::kLacrosOnly}, {});
#endif

  ::testing::StrictMock<MockAppMenuIconControllerDelegate> mock_delegate;

  AppMenuIconController controller(upgrade_detector(), profile(),
                                   &mock_delegate);

  ::testing::InSequence sequence;

  if (!browser_defaults::kShowUpgradeMenuItem) {
    // In ChromeOS, upgrade menu is used for triggering Lacros data migration.
    EXPECT_CALL(mock_delegate,
                UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                    AppMenuIconController::IconType::UPGRADE_NOTIFICATION,
                    AppMenuIconController::Severity::LOW}))
        .Times(6);
  } else {
    if (IsUnstableChannel()) {
      // For dev and canary channels, the upgrade notification should be sent
      // out at a low level for every annoyance level.
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::UPGRADE_NOTIFICATION,
                      AppMenuIconController::Severity::LOW}))
          .Times(5);
    } else {
      // For stable and beta channels, the "none" type and severity should be
      // sent for the "very low" annoyance level, and the ordinary corresponding
      // severity for each other annoyance level ("high" is reported for both
      // the "grace" and "high" annoyance levels).
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::NONE,
                      AppMenuIconController::Severity::NONE}));
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::UPGRADE_NOTIFICATION,
                      AppMenuIconController::Severity::LOW}));
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::UPGRADE_NOTIFICATION,
                      AppMenuIconController::Severity::MEDIUM}));
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::UPGRADE_NOTIFICATION,
                      AppMenuIconController::Severity::HIGH}))
          .Times(2);
    }
    EXPECT_CALL(mock_delegate,
                UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                    AppMenuIconController::IconType::NONE,
                    AppMenuIconController::Severity::NONE}));
  }

  BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_GRACE);
  BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
}

#if BUILDFLAG(IS_WIN)
INSTANTIATE_TEST_SUITE_P(
    All,
    AppMenuIconControllerTest,
    ::testing::Range(0, static_cast<int>(install_static::NUM_INSTALL_MODES)));
#else
INSTANTIATE_TEST_SUITE_P(All, AppMenuIconControllerTest, ::testing::Values(0));
#endif
