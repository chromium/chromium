// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/view_class_properties.h"

class ToolbarControllerTest : public InteractiveBrowserTest {
 public:
  ToolbarControllerTest() {
    scoped_feature_list_.InitWithFeatures({features::kResponsiveToolbar}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ToolbarControllerTest, FlexOrderCorrect) {
  const ToolbarController* toolbar_controller =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->toolbar_controller();
  const std::vector<ui::ElementIdentifier> element_ids =
      toolbar_controller->element_ids_;
  int element_flex_order_start = toolbar_controller->element_flex_order_start_;

  for (ui::ElementIdentifier id : element_ids) {
    const views::View* toolbar_element =
        toolbar_controller->FindToolbarElementWithId(id);
    if (toolbar_element) {
      EXPECT_EQ(element_flex_order_start++,
                toolbar_element->GetProperty(views::kFlexBehaviorKey)->order());
    }
  }
}
