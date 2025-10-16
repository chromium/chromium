// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/search_engines/dse_reset_dialog.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
namespace {

// Returns the DSE reset bubble if it is currently showing, otherwise nullptr.
views::BubbleDialogDelegate* GetDseResetBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }

  // Iterate over all active widgets to find the bubble anchored to the app
  // menu. This is a standard approach for testing bubbles in a browser test
  // environment.
  for (views::Widget* widget : views::test::WidgetTest::GetAllWidgets()) {
    if (!widget->IsVisible()) {
      continue;
    }
    const std::u16string expected_title = l10n_util::GetStringUTF16(
        IDS_DEFAULT_SEARCH_ENGINE_RESET_NOTIFICATION_TITLE);
    auto* bubble_delegate = widget->widget_delegate()->AsBubbleDialogDelegate();
    if (bubble_delegate &&
        bubble_delegate->GetWindowTitle() == expected_title) {
      return bubble_delegate;
    }
  }
  return nullptr;
}

void Click(views::View* clickable_view) {
  // Simulate a mouse click. Note: Buttons are either fired when pressed or
  // when released, so the corresponding methods need to be called.
  clickable_view->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  clickable_view->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
}

bool GetShowDefaultSearchEngineResetNotificationValue(Browser* browser) {
  return browser->profile()->GetPrefs()->GetBoolean(
      prefs::kShowDefaultSearchEngineResetNotification);
}

}  // namespace

class DseResetDialogBrowserTest : public DialogBrowserTest {
 public:
  DseResetDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(
        switches::kResetTamperedDefaultSearchEngine);
  }

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kShowDefaultSearchEngineResetNotification, true);
  }

  void ShowUi(const std::string& name) override {
    search_engines::MaybeShowSearchEngineResetNotification(
        browser(), AutocompleteMatch::Type::SEARCH_WHAT_YOU_TYPED);

    views::BubbleDialogDelegate* bubble = GetDseResetBubble(browser());
    ASSERT_NE(nullptr, bubble);
    ASSERT_NE(nullptr, bubble->GetWidget());

    views::test::WidgetVisibleWaiter(bubble->GetWidget()).Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies the dialog is shown correctly using the DialogBrowserTest
// framework.
IN_PROC_BROWSER_TEST_F(DseResetDialogBrowserTest, ShowAndVerifyUi) {
  base::HistogramTester histograms;
  ASSERT_TRUE(GetShowDefaultSearchEngineResetNotificationValue(browser()));

  ShowAndVerifyUi();
  EXPECT_FALSE(GetShowDefaultSearchEngineResetNotificationValue(browser()));

  histograms.ExpectUniqueSample(
      "Search.DefaultSearchEngineResetNotificationShown", true, 1);
}

// Verifies the "Got It" button closes the dialog.
IN_PROC_BROWSER_TEST_F(DseResetDialogBrowserTest, GotItButtonClosesDialog) {
  ShowUi("default");

  views::BubbleDialogDelegate* bubble = GetDseResetBubble(browser());
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  // The "Got It" button is the dialog's OK button.
  bubble->AcceptDialog();
  waiter.Wait();

  EXPECT_EQ(nullptr, GetDseResetBubble(browser()));
}

// Verifies the "Learn More" button opens a new tab with the correct URL.
IN_PROC_BROWSER_TEST_F(DseResetDialogBrowserTest, LearnMoreButtonOpensNewTab) {
  ShowUi("default");

  views::BubbleDialogDelegate* bubble = GetDseResetBubble(browser());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // The "Learn More" button is the dialog's "extra" view.
  views::View* learn_more_button = bubble->GetExtraView();
  ASSERT_NE(nullptr, learn_more_button);

  Click(learn_more_button);

  tab_waiter.Wait();

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL kExpectedLearnMoreUrl(
      "https://support.google.com/chrome/answer/"
      "3296214#zippy=%2Cchrome-reset-my-browser-settings");
  EXPECT_EQ(kExpectedLearnMoreUrl, new_tab->GetVisibleURL());
  EXPECT_FALSE(GetShowDefaultSearchEngineResetNotificationValue(browser()));

  // The dialog should not close when the learn more link is clicked.
  EXPECT_NE(nullptr, GetDseResetBubble(browser()));
}

// Verifies the dialog is not shown if the controlling pref is false.
IN_PROC_BROWSER_TEST_F(DseResetDialogBrowserTest, DialogNotShownIfPrefIsFalse) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowDefaultSearchEngineResetNotification, false);

  search_engines::MaybeShowSearchEngineResetNotification(
      browser(), AutocompleteMatch::Type::SEARCH_WHAT_YOU_TYPED);

  EXPECT_EQ(nullptr, GetDseResetBubble(browser()));
}

// Verifies the dialog is not shown for non-search match types (e.g., a URL).
IN_PROC_BROWSER_TEST_F(DseResetDialogBrowserTest, DialogNotShownForUrlMatch) {
  search_engines::MaybeShowSearchEngineResetNotification(
      browser(), AutocompleteMatch::Type::URL_WHAT_YOU_TYPED);
  EXPECT_EQ(nullptr, GetDseResetBubble(browser()));
}

// Test fixture where the controlling feature is disabled.
class DseResetDialogFeatureDisabledBrowserTest : public InProcessBrowserTest {
 public:
  DseResetDialogFeatureDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        switches::kResetTamperedDefaultSearchEngine);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies the dialog is not shown when the feature flag is disabled.
IN_PROC_BROWSER_TEST_F(DseResetDialogFeatureDisabledBrowserTest,
                       DialogNotShown) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowDefaultSearchEngineResetNotification, true);

  search_engines::MaybeShowSearchEngineResetNotification(
      browser(), AutocompleteMatch::Type::SEARCH_WHAT_YOU_TYPED);

  EXPECT_EQ(nullptr, GetDseResetBubble(browser()));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
