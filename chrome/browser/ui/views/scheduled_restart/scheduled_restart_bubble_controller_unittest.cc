// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_controller.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
  void set_should_fail_show_bubble(bool fail) {
    should_fail_show_bubble_ = fail;
  }

  void SetWidget(std::unique_ptr<views::Widget> widget) {
    widget_ = std::move(widget);
  }

 protected:
  views::Widget* ShowBubble(BrowserWindowInterface* browser) override {
    ++bubble_shown_count_;
    return should_fail_show_bubble_ ? nullptr : widget_.get();
  }

 private:
  int bubble_shown_count_ = 0;
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
    feature_list_.InitAndEnableFeature(features::kScheduledRestart);
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

}  // namespace scheduled_restart
