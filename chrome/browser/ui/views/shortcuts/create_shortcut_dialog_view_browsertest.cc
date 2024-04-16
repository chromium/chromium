// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace shortcuts {

namespace {

class CreateShortcutDialogViewBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest overrides:
  void ShowUi(const std::string& name) override {
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

    std::u16string title;
    if (name != "empty") {
      title = u"ABC";
    }
    chrome::ShowCreateShortcutDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), gfx::ImageSkia(),
        title, base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest, InvokeUiBasic) {
  base::UserActionTester action_tester;
  ShowAndVerifyUi();
  EXPECT_EQ(1,
            action_tester.GetActionCount("CreateDesktopShortcutDialogShown"));
}

// Dialog destruction due to navigations or other reasons are measured as
// cancellations from an user action perspective.
IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest,
                       InvokeUi_WidgetDestroyedOnNavigation) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi(base::EmptyString());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest,
                       InvokeUi_WidgetClosesOnVisibilityChange) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi(base::EmptyString());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Navigate to a new tab.
  chrome::NewTab(browser());

  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest,
                       InvokeUi_WidgetClosesOnWebContentsDestruction) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi(base::EmptyString());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Close();

  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest, InvokeUi_Accept) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi(base::EmptyString());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogAccepted"));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest,
                       InvokeUi_Cancel_TitleFilled) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi(base::EmptyString());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutDialogViewBrowserTest,
                       InvokeUi_Cancel_TitleEmpty) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi("empty");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

}  // namespace

}  // namespace shortcuts
