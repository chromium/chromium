// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_fre_dialog.h"

#include <memory>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ttc {

namespace {

class TtcFreDialogBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  std::unique_ptr<TtcFreDialog> ShowDialog() {
    return std::make_unique<TtcFreDialog>(
        CHECK_DEREF(browser()->GetActiveTabInterface()), on_closed_.Get(),
        on_learn_more_.Get());
  }

  template <typename T>
  T* GetView(TtcFreDialog& dialog, ui::ElementIdentifier id) {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
        id, views::ElementTrackerViews::GetContextForWidget(
                dialog.widget_for_testing()));
  }

  base::MockCallback<TtcFreDialog::ResultCallback> on_closed_;
  base::MockRepeatingClosure on_learn_more_;
};

IN_PROC_BROWSER_TEST_F(TtcFreDialogBrowserTest, AllowButtonAccepts) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  auto* const allow_button =
      GetView<views::Button>(*dialog, TtcFreDialog::kAllowButtonElementId);
  ASSERT_TRUE(allow_button);
  EXPECT_CALL(on_closed_, Run(true)).WillOnce([&dialog](bool) {
    dialog.reset();
  });
  views::test::ButtonTestApi(allow_button).NotifyDefaultMouseClick();
  EXPECT_EQ(dialog, nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcFreDialogBrowserTest, CancelButtonDeclines) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  auto* const cancel_button =
      GetView<views::Button>(*dialog, TtcFreDialog::kCancelButtonElementId);
  ASSERT_TRUE(cancel_button);
  EXPECT_CALL(on_closed_, Run(false));
  views::test::ButtonTestApi(cancel_button).NotifyDefaultMouseClick();
  EXPECT_EQ(dialog->widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcFreDialogBrowserTest, LearnMoreRunsCallback) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  auto* const link =
      GetView<views::Link>(*dialog, TtcFreDialog::kLearnMoreLinkElementId);
  ASSERT_TRUE(link);
  EXPECT_CALL(on_learn_more_, Run()).Times(2);
  EXPECT_CALL(on_closed_, Run).Times(0);
  const ui::KeyEvent space(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  link->OnKeyPressed(space);
  link->OnKeyPressed(space);
  EXPECT_NE(dialog->widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcFreDialogBrowserTest, ClosingTabDeclines) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog->widget_for_testing());

  EXPECT_CALL(on_closed_, Run(false));
  CHECK_DEREF(dialog->tab()).Close();
  destroyed_waiter.Wait();
  EXPECT_EQ(dialog->widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcFreDialogBrowserTest,
                       CrossSiteNavigationKeepsDialogOpen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  EXPECT_CALL(on_closed_, Run).Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.test", "/title1.html")));
  EXPECT_NE(dialog->widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcFreDialogBrowserTest,
                       DestroyingClosesWithoutCallback) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog->widget_for_testing());

  EXPECT_CALL(on_closed_, Run).Times(0);
  dialog.reset();
  destroyed_waiter.Wait();
}

}  // namespace

}  // namespace ttc
