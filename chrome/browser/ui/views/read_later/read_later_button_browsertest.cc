// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/read_later/read_later_button.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/read_later/read_later_test_utils.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/dot_indicator.h"

class ReadLaterButtonBrowserTest : public DialogBrowserTest {
 public:
  ReadLaterButtonBrowserTest() {
    feature_list_.InitAndEnableFeature(reading_list::switches::kReadLater);
  }

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);
    // Navigate to a url that can be added to the reading list.
    ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com"));
  }

  ReadLaterButton* GetReadLaterButton(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->bookmark_bar()
        ->read_later_button();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(browser()->bookmark_bar_state() == BookmarkBar::SHOW);
    ClickReadLaterButton();
  }

  void ClickReadLaterButton() {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    GetReadLaterButton(browser())->OnMousePressed(click_event);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(ReadLaterButtonBrowserTest);
};

// TODO(1115950): Flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_F(ReadLaterButtonBrowserTest, MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ReadLaterButtonBrowserTest,
                       DotIndicatorVisibleWithUnreadItems) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser()->profile());
  test::ReadingListLoadObserver(model).Wait();

  // Verify the dot indicator is seen when there is an unseen entry.
  model->AddEntry(GURL("http://foo/1"), "Tab 1",
                  reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
  ASSERT_TRUE(
      GetReadLaterButton(browser())->dot_indicator_for_testing()->GetVisible());

  // Verify the dot indicator is hidden once the reading list is opened.
  ClickReadLaterButton();
  ASSERT_FALSE(
      GetReadLaterButton(browser())->dot_indicator_for_testing()->GetVisible());
}
