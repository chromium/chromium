// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class PopupSeparatorViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    view_ = widget_->SetContentsView(
        std::make_unique<PopupSeparatorView>(/*vertical_padding=*/1));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupSeparatorView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PopupSeparatorView> view_ = nullptr;
};

TEST_F(PopupSeparatorViewTest, FocusBehavior) {
  EXPECT_EQ(view().GetFocusBehavior(), views::View::FocusBehavior::NEVER);
}

TEST_F(PopupSeparatorViewTest, AccessibleProperties) {
  ui::AXNodeData node_data;
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kSplitter, node_data.role);
}

}  // namespace autofill
