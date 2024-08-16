// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_title_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

class PopupTitleViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    view_ = std::make_unique<PopupTitleView>(/*title=*/u"Some title");
  }

 protected:
  PopupTitleView& view() { return *view_; }

 private:
  std::unique_ptr<PopupTitleView> view_ = nullptr;
};

TEST_F(PopupTitleViewTest, AccessibleProperties) {
  ui::AXNodeData node_data;
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kLabelText, node_data.role);
}

}  // namespace autofill
