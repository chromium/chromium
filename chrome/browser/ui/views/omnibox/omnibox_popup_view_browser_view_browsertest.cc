// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_browser_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

class OmniboxPopupViewBrowserViewTest : public InteractiveBrowserTest {
 public:
  OmniboxPopupViewBrowserViewTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        omnibox::kWebUIOmniboxFullPopupV2,
        {{"Omnibox_UseBrowserView", "true"}});
  }

 protected:
  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewBrowserViewTest, CreatesView) {
  OmniboxPopupView* popup_view = location_bar()->GetOmniboxPopupView();
  ASSERT_TRUE(popup_view);

  // In full popup mode, it may be open immediately if focused.
  EXPECT_TRUE(popup_view->IsOpen());
}
