// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestDefaultBrowserSurfaceManager : public DefaultBrowserSurfaceManager {
 public:
  explicit TestDefaultBrowserSurfaceManager(
      default_browser::DefaultBrowserEntrypointType entrypoint_type =
          default_browser::DefaultBrowserEntrypointType::kStartupInfobar)
      : entrypoint_type_(entrypoint_type) {}
  ~TestDefaultBrowserSurfaceManager() override = default;

  using DefaultBrowserSurfaceManager::IsBrowserValidForShowing;

  default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const override {
    return entrypoint_type_;
  }

  void ShowForBrowser(BrowserWindowInterface* browser) override {}
  void CloseForBrowser(BrowserWindowInterface* browser) override {}
  void CloseAllPromptInstances() override {}

 private:
  default_browser::DefaultBrowserEntrypointType entrypoint_type_;
};

class DefaultBrowserSurfaceManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    browser_window_interface_ =
        std::make_unique<::testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));
    ON_CALL(*browser_window_interface_, GetType())
        .WillByDefault(::testing::Return(BrowserWindowInterface::TYPE_NORMAL));
  }

  TestingProfile* profile() { return profile_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
};

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForNormalProfile) {
  TestDefaultBrowserSurfaceManager manager;
  EXPECT_TRUE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForIncognitoProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(otr_profile));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest,
       IsBrowserValidForEnterpriseIsolatedMode) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile_metrics::SetBrowserProfileType(
      otr_profile, profile_metrics::BrowserProfileType::kEnterpriseIsolated);
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(otr_profile));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForGuestSession) {
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();
  ON_CALL(*browser_window_interface(), GetProfile())
      .WillByDefault(::testing::Return(guest_profile.get()));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, IsBrowserValidForNonNormalWindow) {
  ON_CALL(*browser_window_interface(), GetType())
      .WillByDefault(::testing::Return(BrowserWindowInterface::TYPE_POPUP));

  TestDefaultBrowserSurfaceManager manager;
  EXPECT_FALSE(manager.IsBrowserValidForShowing(browser_window_interface()));
}

TEST_F(DefaultBrowserSurfaceManagerTest, HandleDismissIncrementsDeclinedCount) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kDefaultBrowserDeclinedCount, 0);

  TestDefaultBrowserSurfaceManager manager;
  manager.Show(/*can_pin_to_taskbar=*/false);
  manager.HandleDismiss();

  EXPECT_EQ(local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount), 1);
}

TEST_F(DefaultBrowserSurfaceManagerTest,
       HandleDismissAfterAcceptIncrementsDeclinedCountAndRecordsDismissed) {
  base::HistogramTester histogram_tester;
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kDefaultBrowserDeclinedCount, 0);

  TestDefaultBrowserSurfaceManager manager;
  manager.Show(/*can_pin_to_taskbar=*/false);
  manager.HandleAccept();
  manager.HandleDismiss();

  EXPECT_EQ(local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount), 1);
  histogram_tester.ExpectBucketCount(
      "DefaultBrowser.InfoBar.ShellIntegration.Interaction",
      default_browser::DefaultBrowserInteractionType::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      "DefaultBrowser.InfoBar.ShellIntegration.Interaction",
      default_browser::DefaultBrowserInteractionType::kDismissed, 1);
}

TEST_F(DefaultBrowserSurfaceManagerTest, HandleAcceptNotifiesObservers) {
  TestDefaultBrowserSurfaceManager manager;
  manager.Show(/*can_pin_to_taskbar=*/false);
  EXPECT_FALSE(manager.has_accepted());

  std::vector<bool> observed;
  base::CallbackListSubscription subscription =
      manager.RegisterHasAcceptedChanged(
          base::BindLambdaForTesting([&observed](bool has_accepted) {
            observed.push_back(has_accepted);
          }));

  manager.HandleAccept();
  EXPECT_TRUE(manager.has_accepted());
  EXPECT_THAT(observed, ::testing::ElementsAre(true));

  // Subsequent accepts are no-ops and do not re-notify.
  manager.HandleAccept();
  EXPECT_THAT(observed, ::testing::ElementsAre(true));

  manager.Show(/*can_pin_to_taskbar=*/false);
  EXPECT_FALSE(manager.has_accepted());
}

TEST_F(DefaultBrowserSurfaceManagerTest, RecordsRetryCountOnCloseAfterAccept) {
  base::HistogramTester histogram_tester;

  // Non-sticky surfaces do not emit RetryCount.
  TestDefaultBrowserSurfaceManager non_sticky_manager(
      default_browser::DefaultBrowserEntrypointType::kStartupInfobar);
  non_sticky_manager.Show(/*can_pin_to_taskbar=*/false);
  non_sticky_manager.HandleAccept();
  non_sticky_manager.CloseAll();
  histogram_tester.ExpectTotalCount(
      "DefaultBrowser.InfoBar.ShellIntegration.RetryCount", 0);

  TestDefaultBrowserSurfaceManager sticky_manager(
      default_browser::DefaultBrowserEntrypointType::
          kStickyModalDialogWithoutSettingsIllustration);
  constexpr char kStickyHistogram[] =
      "DefaultBrowser.StickyModalDialogWithoutSettingsIllustration."
      "ShellIntegration.RetryCount";

  sticky_manager.Show(/*can_pin_to_taskbar=*/false);
  sticky_manager.HandleRetry();
  sticky_manager.HandleDismiss();
  sticky_manager.CloseAll();
  histogram_tester.ExpectTotalCount(kStickyHistogram, 0);

  sticky_manager.Show(/*can_pin_to_taskbar=*/false);
  sticky_manager.HandleAccept();
  sticky_manager.HandleRetry();
  sticky_manager.HandleRetry();
  sticky_manager.CloseAll();
  histogram_tester.ExpectBucketCount(kStickyHistogram, 2, 1);
  // Recording the retry count does not change the accepted state.
  EXPECT_TRUE(sticky_manager.has_accepted());

  // Retrying 5 times caps into the 3+ bucket.
  sticky_manager.Show(/*can_pin_to_taskbar=*/false);
  sticky_manager.HandleAccept();
  for (int i = 0; i < 5; ++i) {
    sticky_manager.HandleRetry();
  }
  sticky_manager.CloseAll();
  histogram_tester.ExpectBucketCount(kStickyHistogram, 3, 1);
  histogram_tester.ExpectTotalCount(kStickyHistogram, 2);
}
