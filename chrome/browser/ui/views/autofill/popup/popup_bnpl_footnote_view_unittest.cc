// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_bnpl_footnote_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class PopupBnplFootnoteViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void ShowView() {
    view_ = widget_->SetContentsView(
        std::make_unique<PopupBnplFootnoteView>(controller_.GetWeakPtr()));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupBnplFootnoteView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

  testing::NiceMock<MockAutofillPopupController> controller_;

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PopupBnplFootnoteView> view_ = nullptr;
};

TEST_F(PopupBnplFootnoteViewTest, AccessibilityWrapperRole) {
  ShowView();
  ui::AXNodeData node_data;

  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);

  // Check that PopupBnplFootnoteView is a layout wrapper, ensuring that the
  // child `views::StyledLabel` handles the text and link accessibility
  // natively.
  EXPECT_EQ(ax::mojom::Role::kUnknown, node_data.role);
}

TEST_F(PopupBnplFootnoteViewTest, PreferredSizeWidthIsZeroWhenUnbounded) {
  ShowView();

  views::SizeBounds unbounded_size;
  gfx::Size preferred_size = view().CalculatePreferredSize(unbounded_size);

  EXPECT_EQ(0, preferred_size.width());
}

}  // namespace autofill
