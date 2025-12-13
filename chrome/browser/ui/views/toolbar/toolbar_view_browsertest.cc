// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"

class ToolbarViewUnitTest : public InProcessBrowserTest {
 public:
  ToolbarButton* GetForwardButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->forward_button();
  }
};

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, ForwardButtonVisibility) {
  // Forward button should be visible by default.
  EXPECT_TRUE(GetForwardButton()->GetVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  browser_view->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                     false);
  EXPECT_FALSE(GetForwardButton()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, AccessibleProperties) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ui::AXNodeData data;

  toolbar->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
}
