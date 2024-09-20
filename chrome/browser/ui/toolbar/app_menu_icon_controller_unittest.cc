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
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
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

  FakeUpgradeDetector upgrade_detector_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Tests that the controller's delegate is notified with the proper icon type
// and severity when an upgrade is detected.
TEST_P(AppMenuIconControllerTest, UpgradeNotification) {
  ::testing::StrictMock<MockAppMenuIconControllerDelegate> mock_delegate;

  AppMenuIconController controller(upgrade_detector(), profile(),
                                   &mock_delegate);

  ::testing::InSequence sequence;

  if (!browser_defaults::kShowUpgradeMenuItem) {
    // ChromeOS doesn't change the icon.
    EXPECT_CALL(mock_delegate,
                UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                    AppMenuIconController::IconType::NONE,
                    AppMenuIconController::Severity::NONE}))
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

#if !BUILDFLAG(IS_CHROMEOS)
class AppMenuControllerDefaultPromptTest : public BrowserWithTestWindowTest {
 public:
  AppMenuControllerDefaultPromptTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDefaultBrowserPromptRefresh,
        {{features::kShowDefaultBrowserAppMenuChip.name, "true"}});
    BrowserWithTestWindowTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppMenuControllerDefaultPromptTest, SetsDefaultPromptTypeAndSeverity) {
  ::testing::StrictMock<MockAppMenuIconControllerDelegate> mock_delegate;
  AppMenuIconController controller(profile(), &mock_delegate);

  EXPECT_CALL(mock_delegate,
              UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                  AppMenuIconController::IconType::DEFAULT_BROWSER_PROMPT,
                  AppMenuIconController::Severity::LOW}));
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
}
#endif
