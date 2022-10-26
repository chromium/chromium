// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

class InteractiveBrowserTestBrowsertest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
  }

  void TearDownOnMainThread() override {
    browser_view_ = nullptr;
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::raw_ptr<BrowserView> browser_view_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, SelectTab) {
  // Add at least three tabs.
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  RunTestSequence(
      SelectTab(kTabStripElementId, 1), Check(base::BindLambdaForTesting([&]() {
        return browser_view_->tabstrip()->GetActiveIndex() == 1;
      })),
      SelectTab(kTabStripElementId, 2), Check(base::BindLambdaForTesting([&]() {
        return browser_view_->tabstrip()->GetActiveIndex() == 2;
      })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckElement) {
  RunTestSequence(CheckElement(
      kAppMenuButtonElementId,
      base::BindLambdaForTesting([&](ui::TrackedElement* el) {
        return el->IsA<views::TrackedElementViews>() &&
               el->context() == browser()->window()->GetElementContext();
      })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckView) {
  RunTestSequence(
      CheckView(kAppMenuButtonElementId,
                base::BindOnce([](BrowserAppMenuButton* button) {
                  return button->GetVisible();
                })),
      CheckView(kTabStripElementId, base::BindOnce([](TabStrip* tab_strip) {
                  return tab_strip->GetTabCount() == 1;
                })));
}

IN_PROC_BROWSER_TEST_F(InteractiveBrowserTestBrowsertest, CheckViewProperty) {
  RunTestSequence(
      PressButton(kAppMenuButtonElementId),
      CheckViewProperty(AppMenuModel::kMoreToolsMenuItem,
                        &views::MenuItemView::title,
                        // Implicit creation of an equality matcher.
                        l10n_util::GetStringUTF16(IDS_MORE_TOOLS_MENU)),
      CheckViewProperty(
          AppMenuModel::kDownloadsMenuItem, &views::MenuItemView::title,
          // Explicit creation of an inequality matcher.
          testing::Ne(l10n_util::GetStringUTF16(IDS_MORE_TOOLS_MENU))));
}
