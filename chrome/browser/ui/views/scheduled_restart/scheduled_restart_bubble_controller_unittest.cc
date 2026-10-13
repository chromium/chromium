// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_controller.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/config/linux/dbus/buildflags.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/lifetime/scheduled_restart_test_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#endif

namespace scheduled_restart {

namespace {

class TestScheduledRestartBubbleController
    : public ScheduledRestartBubbleController {
 public:
  explicit TestScheduledRestartBubbleController(
      ScheduledRestartManager* manager = nullptr) {
    if (manager) {
      set_scheduled_restart_manager_for_testing(manager);
    }
  }

  ~TestScheduledRestartBubbleController() override {
    if (widget_) {
      widget_->CloseNow();
    }
  }

  int bubble_shown_count() const { return bubble_shown_count_; }
  bool last_show_active() const { return last_show_active_; }
  void set_should_fail_show_bubble(bool fail) {
    should_fail_show_bubble_ = fail;
  }

  void SetWidget(std::unique_ptr<views::Widget> widget) {
    widget_ = std::move(widget);
  }

 protected:
  std::unique_ptr<views::Widget> ShowBubble(
      BrowserWindowInterface* browser,
      views::Widget::ClosedCallback on_close,
      bool show_active) override {
    ++bubble_shown_count_;
    last_show_active_ = show_active;
    if (should_fail_show_bubble_) {
      return nullptr;
    }
    if (widget_ && on_close) {
      widget_->MakeCloseSynchronous(std::move(on_close));
    }
    return std::move(widget_);
  }

 private:
  int bubble_shown_count_ = 0;
  bool last_show_active_ = false;
  bool should_fail_show_bubble_ = false;
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace

class ScheduledRestartBubbleControllerTest : public ChromeViewsTestBase {
 public:
  ScheduledRestartBubbleControllerTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    feature_list_.InitWithFeatures(
        {features::kScheduledRestart, features::kScheduledRestartAppMenuItem},
        {});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSimulateUpgrade);

    upgrade_detector_.SetUpgradeAvailable();
    upgrade_detector_.set_upgrade_notification_stage_for_testing(
        UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
    upgrade_detector_.set_upgrade_detected_time(base::Time::Now() -
                                                base::Days(15));

    scheduled_restart_manager_ =
        std::make_unique<ScheduledRestartManager>(upgrade_detector_);
    local_state()->ClearPref(prefs::kScheduledRestartLastNudgeTime);
  }

  void TearDown() override {
    scheduled_restart_manager_.reset();
    profile_.reset();
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
    // Scheduling a restart starts idle monitoring, which subscribes to screen
    // lock changes over D-Bus. Stop the D-Bus thread so it cannot post replies
    // to the main thread after the task environment checks that it is idle.
    dbus_thread_linux::Shutdown();
#endif
    ChromeViewsTestBase::TearDown();
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  TestingProfile* profile() { return profile_.get(); }
  FakeUpgradeDetector& upgrade_detector() { return upgrade_detector_; }
  ScheduledRestartManager* scheduled_restart_manager() {
    return scheduled_restart_manager_.get();
  }
  MockBrowserWindowInterface* browser_window() { return &browser_window_; }

  std::unique_ptr<views::Widget> CreateBubbleWidget() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget->Init(std::move(params));
    return widget;
  }

  std::unique_ptr<content::WebContents> CreateNtpWebContents() {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
    webui::SetBrowserWindowInterface(web_contents.get(), browser_window());
    return web_contents;
  }

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  FakeUpgradeDetector upgrade_detector_;
  std::unique_ptr<ScheduledRestartManager> scheduled_restart_manager_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_;
};

TEST_F(ScheduledRestartBubbleControllerTest, TriggerOnForegroundNTP) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());
  controller.SetWidget(CreateBubbleWidget());

  auto web_contents = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 1);
  EXPECT_FALSE(controller.last_show_active());
  EXPECT_TRUE(controller.is_bubble_showing());
  EXPECT_FALSE(
      local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime).is_null());
}

TEST_F(ScheduledRestartBubbleControllerTest, NonVisibleTabDoesNotTrigger) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  auto web_contents = CreateNtpWebContents();
  web_contents->WasHidden();

  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
  EXPECT_TRUE(
      local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime).is_null());
}

TEST_F(ScheduledRestartBubbleControllerTest, TabWithOpenerDoesNotTrigger) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  auto opener_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto web_contents = CreateNtpWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->SetOpener(opener_contents.get());

  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
  EXPECT_TRUE(
      local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime).is_null());
}

TEST_F(ScheduledRestartBubbleControllerTest,
       NavigatedTabWithHistoryDoesNotTrigger) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  // Simulate an existing tab with browsing history navigating to NTP.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://www.google.com"));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  webui::SetBrowserWindowInterface(web_contents.get(), browser_window());

  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
  EXPECT_TRUE(
      local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime).is_null());
}

TEST_F(ScheduledRestartBubbleControllerTest, RestoredTabDoesNotTrigger) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(content::NavigationEntry::Create());
  entries.back()->SetURL(GURL(chrome::kChromeUINewTabURL));
  web_contents->GetController().Restore(0, content::RestoreType::kRestored,
                                        &entries);
  webui::SetBrowserWindowInterface(web_contents.get(), browser_window());

  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
  EXPECT_TRUE(
      local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime).is_null());
}

TEST_F(ScheduledRestartBubbleControllerTest,
       DuplicateTriggerSuppressedWhileShowing) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  controller.SetWidget(CreateBubbleWidget());

  auto web_contents1 = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents1.get());
  EXPECT_EQ(controller.bubble_shown_count(), 1);

  // Opening another NTP while bubble is showing does not trigger another
  // bubble.
  auto web_contents2 = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents2.get());
  EXPECT_EQ(controller.bubble_shown_count(), 1);
}

TEST_F(ScheduledRestartBubbleControllerTest, NullWidgetDoesNotRecordNudge) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  controller.set_should_fail_show_bubble(true);

  auto web_contents = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 1);
  // Nudge timestamp should NOT be recorded when widget creation fails.
  EXPECT_EQ(base::Time(),
            local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime));
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AlreadyScheduledSuppressesTrigger) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  scheduled_restart_manager()->ScheduleRestartOnIdle();

  auto web_contents = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents.get());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerShowsBubbleBypassesNudgeDelay) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());
  controller.SetWidget(CreateBubbleWidget());

  // Set upgrade detected time to now so the passive NTP nudge delay (30s under
  // --simulate-upgrade) is not yet met.
  upgrade_detector().set_upgrade_detected_time(base::Time::Now());
  EXPECT_FALSE(scheduled_restart_manager()->ShouldShowNudge());

  controller.ShowBubbleFromAppMenu(browser_window());

  EXPECT_EQ(controller.bubble_shown_count(), 1);
  EXPECT_TRUE(controller.last_show_active());
  EXPECT_FALSE(
      local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime).is_null());
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerBypassesNudgeCooldown) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());
  controller.SetWidget(CreateBubbleWidget());

  // A nudge shown recently puts the passive nudge in its cooldown (30s under
  // --simulate-upgrade), which the explicit App Menu action ignores.
  const base::Time last_nudge_time = base::Time::Now() - base::Seconds(10);
  local_state()->SetTime(prefs::kScheduledRestartLastNudgeTime,
                         last_nudge_time);
  EXPECT_FALSE(scheduled_restart_manager()->ShouldShowNudge());

  controller.ShowBubbleFromAppMenu(browser_window());

  EXPECT_EQ(controller.bubble_shown_count(), 1);
  EXPECT_TRUE(controller.is_bubble_showing());
  EXPECT_GT(local_state()->GetTime(prefs::kScheduledRestartLastNudgeTime),
            last_nudge_time);
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerRestartsPassiveNudgeCooldown) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());
  std::unique_ptr<views::Widget> widget = CreateBubbleWidget();
  views::Widget* widget_ptr = widget.get();
  controller.SetWidget(std::move(widget));

  controller.ShowBubbleFromAppMenu(browser_window());
  ASSERT_EQ(controller.bubble_shown_count(), 1);

  // Dismissing the dialog opened from the App Menu must not lead to an
  // immediate passive nudge on the next New Tab Page.
  widget_ptr->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  EXPECT_FALSE(controller.is_bubble_showing());

  controller.SetWidget(CreateBubbleWidget());
  auto web_contents = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents.get());
  EXPECT_EQ(controller.bubble_shown_count(), 1);
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerReplacesExistingPassiveBubble) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());
  std::unique_ptr<views::Widget> passive_widget = CreateBubbleWidget();
  base::WeakPtr<views::Widget> passive_widget_weak =
      passive_widget->GetWeakPtr();
  controller.SetWidget(std::move(passive_widget));

  // First, show a passive (inactive) bubble via an NTP creation.
  auto web_contents = CreateNtpWebContents();
  controller.MaybeShowNudgeForWebContents(web_contents.get());
  EXPECT_EQ(controller.bubble_shown_count(), 1);
  EXPECT_FALSE(controller.last_show_active());
  EXPECT_TRUE(controller.is_bubble_showing());

  // Invoking the App Menu item while the passive bubble is still showing
  // destroys the existing bubble and replaces it with an active bubble.
  controller.SetWidget(CreateBubbleWidget());
  controller.ShowBubbleFromAppMenu(browser_window());
  EXPECT_FALSE(passive_widget_weak);
  EXPECT_EQ(controller.bubble_shown_count(), 2);
  EXPECT_TRUE(controller.last_show_active());
  EXPECT_TRUE(controller.is_bubble_showing());
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerSuppressedWhenAppMenuFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kScheduledRestartAppMenuItem);

  TestScheduledRestartBubbleController controller(scheduled_restart_manager());
  controller.ShowBubbleFromAppMenu(browser_window());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerSuppressedWhenAlreadyScheduled) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  // Defensive: stub out the relaunch so that a real relaunch cannot happen if
  // this test later adds waits while idle polling is active.
  scheduled_restart_manager()->set_relaunch_callback_for_testing(
      base::DoNothing());
  scheduled_restart_manager()->ScheduleRestartOnIdle();
  controller.ShowBubbleFromAppMenu(browser_window());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerSuppressedWhenNoUpgradeAvailable) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  upgrade_detector().set_upgrade_available(
      FakeUpgradeDetector::UPGRADE_AVAILABLE_NONE);
  controller.ShowBubbleFromAppMenu(browser_window());

  EXPECT_EQ(controller.bubble_shown_count(), 0);
}

TEST_F(ScheduledRestartBubbleControllerTest,
       AppMenuTriggerSuppressedWhenOutdatedInstall) {
  TestScheduledRestartBubbleController controller(scheduled_restart_manager());

  upgrade_detector().set_upgrade_available(
      FakeUpgradeDetector::UPGRADE_NEEDED_OUTDATED_INSTALL);
  controller.ShowBubbleFromAppMenu(browser_window());
  EXPECT_EQ(controller.bubble_shown_count(), 0);

  upgrade_detector().set_upgrade_available(
      FakeUpgradeDetector::UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU);
  controller.ShowBubbleFromAppMenu(browser_window());
  EXPECT_EQ(controller.bubble_shown_count(), 0);
}

}  // namespace scheduled_restart
