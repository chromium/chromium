// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_view.h"

#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace scheduled_restart {

class ScheduledRestartBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  ScheduledRestartBubbleViewBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kScheduledRestart);
  }

  void TearDownOnMainThread() override {
    ScheduledRestartBubbleView::set_relaunch_callback_for_testing(
        base::RepeatingClosure());
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ScheduledRestartManager* manager() {
    return g_browser_process->GetFeatures()->scheduled_restart_manager();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleViewBrowserTest,
                       ShowAndVerifyDialogElements) {
  base::UserActionTester user_action_tester;
  auto widget = ScheduledRestartBubbleView::ShowBubble(browser());
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  EXPECT_EQ(
      dialog_delegate->GetWindowTitle(),
      l10n_util::GetPluralStringFUTF16(IDS_RELAUNCH_RECOMMENDED_TITLE, 0));
  EXPECT_TRUE(dialog_delegate->ShouldShowCloseButton());

  // Verify prominent OK button is "Restart now" and default.
  EXPECT_TRUE(
      dialog_delegate->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(dialog_delegate->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
            l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_RESTART_NOW));
  EXPECT_EQ(dialog_delegate->GetDialogButtonStyle(ui::mojom::DialogButton::kOk),
            ui::ButtonStyle::kProminent);
  EXPECT_EQ(dialog_delegate->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kOk));

  // Verify tonal ExtraView button is "Restart when idle".
  auto* idle_button =
      views::AsViewClass<views::MdTextButton>(dialog_delegate->GetExtraView());
  ASSERT_TRUE(idle_button);
  EXPECT_EQ(
      idle_button->GetText(),
      l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_RESTART_WHEN_IDLE));
  EXPECT_EQ(idle_button->GetStyle(), ui::ButtonStyle::kTonal);

  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ScheduledRestart_BubbleShown"));

  widget->CloseNow();
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleViewBrowserTest,
                       RestartNowAcceptsAndTriggersRelaunch) {
  base::UserActionTester user_action_tester;
  bool relaunch_called = false;
  ScheduledRestartBubbleView::set_relaunch_callback_for_testing(
      base::BindLambdaForTesting([&]() { relaunch_called = true; }));

  auto widget = ScheduledRestartBubbleView::ShowBubble(browser());
  ASSERT_TRUE(widget);

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  dialog_delegate->AcceptDialog();

  EXPECT_TRUE(relaunch_called);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ScheduledRestart_RestartNow"));
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleViewBrowserTest,
                       RestartWhenIdleSchedulesRestart) {
  base::UserActionTester user_action_tester;
  auto widget = ScheduledRestartBubbleView::ShowBubble(browser());
  ASSERT_TRUE(widget);

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  auto* idle_button =
      views::AsViewClass<views::MdTextButton>(dialog_delegate->GetExtraView());
  ASSERT_TRUE(idle_button);

  views::test::ButtonTestApi(idle_button).NotifyDefaultMouseClick();

  EXPECT_TRUE(manager()->is_scheduled());
  EXPECT_EQ(manager()->mode(), ScheduledRestartMode::kOnIdle);
  EXPECT_EQ(1, user_action_tester.GetActionCount("ScheduledRestart_Scheduled"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("ScheduledRestart_Close"));

  auto* toast_controller = ToastController::From(browser());
  ASSERT_TRUE(toast_controller);
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(ToastId::kScheduledRestartOnIdle,
            toast_controller->GetCurrentToastId());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartBubbleViewBrowserTest,
                       DismissBubbleDoesNotScheduleOrRestart) {
  base::UserActionTester user_action_tester;
  auto widget = ScheduledRestartBubbleView::ShowBubble(browser());
  ASSERT_TRUE(widget);

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  EXPECT_FALSE(manager()->is_scheduled());
  EXPECT_EQ(1, user_action_tester.GetActionCount("ScheduledRestart_Close"));
}

}  // namespace scheduled_restart
