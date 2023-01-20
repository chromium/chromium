// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/home_button.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"

class HomeButtonUiTest : public DialogBrowserTest {
 public:
  HomeButtonUiTest() = default;
  HomeButtonUiTest(const HomeButtonUiTest&) = delete;
  HomeButtonUiTest& operator=(const HomeButtonUiTest&) = delete;
  ~HomeButtonUiTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* const prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kShowHomeButton, true);
    auto* home_button = BrowserView::GetBrowserViewForBrowser(browser())
                            ->toolbar()
                            ->home_button();
    HomePageUndoBubbleCoordinator coordinator(home_button, prefs);
    coordinator.Show(GURL(), false);
  }
};

IN_PROC_BROWSER_TEST_F(HomeButtonUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
