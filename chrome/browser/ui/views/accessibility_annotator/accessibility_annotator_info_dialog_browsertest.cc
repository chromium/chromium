// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace accessibility_annotator::info {

namespace {
// This script is used to click a button in the dialog. The script waits for
// the accessibility-annotator-info element and its shadow root button to
// exist before clicking it using a Promise and a polling loop (e.g.
// `setTimeout`).
constexpr char kClickButtonScriptTemplate[] = R"(
  new Promise((resolve) => {
    const interval = setInterval(() => {
      const annotatorInfo =
          document.querySelector('accessibility-annotator-info');
      if (annotatorInfo && annotatorInfo.shadowRoot) {
        const button = annotatorInfo.shadowRoot.querySelector('%s');
        if (button) {
          clearInterval(interval);
          resolve(true);
          setTimeout(() => button.click(), 0);
        }
      }
    }, 50);
  });
)";

}  // namespace

class AccessibilityAnnotatorInfoDialogBrowserTest
    : public InProcessBrowserTest {
 public:
  AccessibilityAnnotatorInfoDialogBrowserTest() = default;
  ~AccessibilityAnnotatorInfoDialogBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       InvokeUi_default) {
  base::HistogramTester histogram_tester;
  std::string histogram_name = "AccessibilityAnnotator.RemoteAnnotatorInfo";

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  histogram_tester.ExpectTotalCount(histogram_name, 0);

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);

  navigation_observer.Wait();
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  histogram_tester.ExpectTotalCount(histogram_name, 1);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     InfoShowRequestResult::kShown, 1);

  controller->CloseDialog();
}

IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       ManageSettingsClickOpensNewTab) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);

  navigation_observer.Wait();
  views::test::WidgetVisibleWaiter(widget).Wait();

  auto* dialog_view = static_cast<AccessibilityAnnotatorInfoDialog*>(
      widget->widget_delegate()->AsBubbleDialogDelegate());
  content::WebContents* dialog_web_contents =
      dialog_view->web_view()->web_contents();

  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());

  EXPECT_TRUE(content::ExecJs(
      dialog_web_contents,
      base::StringPrintf(kClickButtonScriptTemplate, "#manageSettings")));

  content::WebContents* new_tab = tab_add_waiter.Wait();
  EXPECT_EQ(new_tab->GetVisibleURL(),
            GURL(accessibility_annotator::kAccessibilityAnnotatorSettingsURL));
}

IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       LearnMoreClickOpensNewTab) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);

  navigation_observer.Wait();
  views::test::WidgetVisibleWaiter(widget).Wait();

  auto* dialog_view = static_cast<AccessibilityAnnotatorInfoDialog*>(
      widget->widget_delegate()->AsBubbleDialogDelegate());
  content::WebContents* dialog_web_contents =
      dialog_view->web_view()->web_contents();

  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());

  EXPECT_TRUE(content::ExecJs(
      dialog_web_contents,
      base::StringPrintf(kClickButtonScriptTemplate, "#learnMore a")));

  content::WebContents* new_tab = tab_add_waiter.Wait();
  EXPECT_EQ(new_tab->GetVisibleURL(),
            GURL(accessibility_annotator::kAccessibilityAnnotatorLearnMoreURL));
}

IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       GotItClickDismissesDialog) {
  base::HistogramTester histogram_tester;
  std::string histogram_name = "AccessibilityAnnotator.RemoteAnnotatorInfo";

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);

  navigation_observer.Wait();
  views::test::WidgetVisibleWaiter(widget).Wait();

  auto* dialog_view = static_cast<AccessibilityAnnotatorInfoDialog*>(
      widget->widget_delegate()->AsBubbleDialogDelegate());
  content::WebContents* dialog_web_contents =
      dialog_view->web_view()->web_contents();

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  EXPECT_TRUE(content::ExecJs(
      dialog_web_contents,
      base::StringPrintf(kClickButtonScriptTemplate, "#gotIt")));

  destroyed_waiter.Wait();

  histogram_tester.ExpectTotalCount(histogram_name, 2);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     InfoShowRequestResult::kAccepted, 1);
  EXPECT_FALSE(controller->GetWidgetForTesting());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_ClickOutsideDismissesDialog DISABLED_ClickOutsideDismissesDialog
#else
#define MAYBE_ClickOutsideDismissesDialog ClickOutsideDismissesDialog
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       MAYBE_ClickOutsideDismissesDialog) {
  base::HistogramTester histogram_tester;
  std::string histogram_name = "AccessibilityAnnotator.RemoteAnnotatorInfo";

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);

  navigation_observer.Wait();
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  histogram_tester.ExpectTotalCount(histogram_name, 1);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     InfoShowRequestResult::kShown, 1);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  gfx::NativeWindow root_window = views::GetRootWindow(
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
  ui::test::EventGenerator event_generator(root_window);
  gfx::Point click_point =
      widget->GetWindowBoundsInScreen().origin() - gfx::Vector2d(1, 1);
  event_generator.MoveMouseTo(click_point);
  event_generator.ClickLeftButton();

  destroyed_waiter.Wait();

  histogram_tester.ExpectTotalCount(histogram_name, 2);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     InfoShowRequestResult::kDismissed, 1);

  EXPECT_FALSE(controller->GetWidgetForTesting());
}

}  // namespace accessibility_annotator::info
