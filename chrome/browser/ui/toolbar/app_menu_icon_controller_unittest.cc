// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/config/linux/dbus/buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/global_features_test_support.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "components/dbus/thread_linux/dbus_thread_linux.h"
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

// A minimal menu-bearing GlobalError with configurable severity.
class FakeMenuGlobalError : public GlobalError {
 public:
  explicit FakeMenuGlobalError(Severity severity) : severity_(severity) {}
  FakeMenuGlobalError(const FakeMenuGlobalError&) = delete;
  FakeMenuGlobalError& operator=(const FakeMenuGlobalError&) = delete;
  ~FakeMenuGlobalError() override = default;

  // GlobalError:
  Severity GetSeverity() override { return severity_; }
  bool HasMenuItem() override { return true; }
  int MenuItemCommandID() override { return 1; }
  std::u16string MenuItemLabel() override { return u"fake"; }
  void ExecuteMenuItem(BrowserWindowInterface* /*browser*/) override {}
  bool HasShownBubbleView() override { return false; }
  bool HasBubbleView() override { return false; }
  void ShowBubbleView(BrowserWindowInterface* /*browser*/) override {}
  GlobalErrorBubbleViewBase* GetBubbleView() override { return nullptr; }

 private:
  Severity severity_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class TestGlobalFeatures : public GlobalFeatures {
 public:
  explicit TestGlobalFeatures(UpgradeDetector* upgrade_detector)
      : upgrade_detector_(upgrade_detector) {}
  ~TestGlobalFeatures() override = default;

 protected:
  std::unique_ptr<scheduled_restart::ScheduledRestartManager>
  CreateScheduledRestartManager() override {
    return std::make_unique<scheduled_restart::ScheduledRestartManager>(
        *upgrade_detector_);
  }

 private:
  raw_ptr<UpgradeDetector> upgrade_detector_;
};
#endif

}  // namespace


// A test parameterized on an install mode index. For Google Chrome builds on
// Windows, this allows the test to run for each of the supported side-by-side
// channels. For Chromium builds, there is only the one channel. For non-Win
// builds, there does not appear to be an easy way to run the test as if it were
// a different channel.
class AppMenuIconControllerTest : public ::testing::TestWithParam<int> {
 public:
  AppMenuIconControllerTest(const AppMenuIconControllerTest&) = delete;
  AppMenuIconControllerTest& operator=(const AppMenuIconControllerTest&) =
      delete;

 protected:
  AppMenuIconControllerTest()
#if BUILDFLAG(IS_WIN)
      : install_details_(false, GetParam())
#endif
  {
    CreateProfile();
  }

  void TearDown() override {
    ResetProfile();
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
    dbus_thread_linux::ShutdownOnDBusThreadAndBlock();
#endif
  }

  UpgradeDetector* upgrade_detector() { return &upgrade_detector_; }
  Profile* profile() { return profile_.get(); }
  void ResetProfile() { profile_.reset(); }
  void CreateProfile() { profile_ = std::make_unique<TestingProfile>(); }

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
    // https://crbug.com/40601741.
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
  std::unique_ptr<TestingProfile> profile_;
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
                    AppMenuIconController::IconType::kNone,
                    AppMenuIconController::Severity::kNone}))
        .Times(6);
  } else {
    if (IsUnstableChannel()) {
      // For dev and canary channels, the upgrade notification should be sent
      // out at a low level for every annoyance level.
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::kUpgradeNotification,
                      AppMenuIconController::Severity::kLow}))
          .Times(5);
    } else {
      // For stable and beta channels, the "none" type and severity should be
      // sent for the "very low" annoyance level, and the ordinary corresponding
      // severity for each other annoyance level ("high" is reported for both
      // the "grace" and "high" annoyance levels).
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::kNone,
                      AppMenuIconController::Severity::kNone}));
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::kUpgradeNotification,
                      AppMenuIconController::Severity::kLow}));
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::kUpgradeNotification,
                      AppMenuIconController::Severity::kMedium}));
      EXPECT_CALL(mock_delegate,
                  UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                      AppMenuIconController::IconType::kUpgradeNotification,
                      AppMenuIconController::Severity::kHigh}))
          .Times(2);
    }
    EXPECT_CALL(mock_delegate,
                UpdateTypeAndSeverity(AppMenuIconController::TypeAndSeverity{
                    AppMenuIconController::IconType::kNone,
                    AppMenuIconController::Severity::kNone}));
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
TEST_P(AppMenuIconControllerTest, GlobalErrorLowSeverityShowsActionRequired) {
  ::testing::NiceMock<MockAppMenuIconControllerDelegate> mock_delegate;
  AppMenuIconController controller(upgrade_detector(), profile(),
                                   &mock_delegate);

  auto* error_service = GlobalErrorServiceFactory::GetForProfile(profile());
  FakeMenuGlobalError low(GlobalError::SEVERITY_LOW);
  error_service->AddUnownedGlobalError(&low);

  auto state = controller.GetTypeAndSeverity();
  EXPECT_EQ(state.type, AppMenuIconController::IconType::kGlobalError);
  EXPECT_EQ(state.severity, AppMenuIconController::Severity::kLow);

  error_service->RemoveGlobalError(&low);
}

TEST_P(AppMenuIconControllerTest, GlobalErrorMediumOrHighShowsError) {
  ::testing::NiceMock<MockAppMenuIconControllerDelegate> mock_delegate;
  AppMenuIconController controller(upgrade_detector(), profile(),
                                   &mock_delegate);

  auto* error_service = GlobalErrorServiceFactory::GetForProfile(profile());

  FakeMenuGlobalError medium(GlobalError::SEVERITY_MEDIUM);
  error_service->AddUnownedGlobalError(&medium);
  auto state = controller.GetTypeAndSeverity();
  EXPECT_EQ(state.type, AppMenuIconController::IconType::kGlobalError);
  EXPECT_EQ(state.severity, AppMenuIconController::Severity::kMedium);
  error_service->RemoveGlobalError(&medium);

  FakeMenuGlobalError high(GlobalError::SEVERITY_HIGH);
  error_service->AddUnownedGlobalError(&high);
  state = controller.GetTypeAndSeverity();
  EXPECT_EQ(state.type, AppMenuIconController::IconType::kGlobalError);
  EXPECT_EQ(state.severity, AppMenuIconController::Severity::kHigh);
  error_service->RemoveGlobalError(&high);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_P(AppMenuIconControllerTest,
       ScheduledRestartUpdatesMenuLabelAndClampsColor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kScheduledRestart);

  // Reset profile before reinitializing GlobalFeatures to avoid dangling
  // references in keyed services to the replaced GlobalFeatures.
  ResetProfile();
  {
    test::ScopedGlobalFeaturesOverride features_override(base::BindRepeating(
        [](UpgradeDetector* detector) -> std::unique_ptr<GlobalFeatures> {
          return std::make_unique<TestGlobalFeatures>(detector);
        },
        upgrade_detector()));
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);
    CreateProfile();

    auto* manager = TestingBrowserProcess::GetGlobal()
                        ->GetFeatures()
                        ->scheduled_restart_manager();
    ASSERT_TRUE(manager);

    {
      ::testing::NiceMock<MockAppMenuIconControllerDelegate> mock_delegate;
      AppMenuIconController controller(upgrade_detector(), profile(),
                                       &mock_delegate);

      // Set high upgrade annoyance level.
      BroadcastLevel(UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

      auto state_before = controller.GetTypeAndSeverity();
      EXPECT_EQ(state_before.type,
                AppMenuIconController::IconType::kUpgradeNotification);
      EXPECT_EQ(state_before.severity, AppMenuIconController::Severity::kHigh);

      // Activate Idle scheduled restart.
      manager->ScheduleRestartOnIdle();

      auto state_after = controller.GetTypeAndSeverity();
      EXPECT_EQ(state_after.type,
                AppMenuIconController::IconType::kUpgradeNotification);
      // Severity should be clamped to kLow under Option A.
      EXPECT_EQ(state_after.severity, AppMenuIconController::Severity::kLow);

      // Label and tooltip should show "Restart scheduled".
      std::u16string expected_label =
          l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_RESTART_SCHEDULED);
      EXPECT_EQ(AppMenuIconController::GetIconLabel(state_after.type,
                                                    state_after.severity),
                expected_label);
      EXPECT_EQ(AppMenuIconController::GetIconTooltip(state_after.type,
                                                      state_after.severity),
                expected_label);

      manager->CancelSchedule();
      auto state_cancelled = controller.GetTypeAndSeverity();
      EXPECT_EQ(state_cancelled.type,
                AppMenuIconController::IconType::kUpgradeNotification);
      // Severity should unclamp back to kHigh upon schedule cancellation.
      EXPECT_EQ(state_cancelled.severity,
                AppMenuIconController::Severity::kHigh);
      EXPECT_NE(AppMenuIconController::GetIconLabel(state_cancelled.type,
                                                    state_cancelled.severity),
                expected_label);
      EXPECT_NE(AppMenuIconController::GetIconTooltip(state_cancelled.type,
                                                      state_cancelled.severity),
                expected_label);
    }

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
    dbus_thread_linux::ShutdownOnDBusThreadAndBlock();
#endif

    ResetProfile();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }
  // Restore default GlobalFeatures and profile for subsequent tests.
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);
  CreateProfile();
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif  // !BUILDFLAG(IS_CHROMEOS)
