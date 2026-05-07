// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_bnpl_footnote_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
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
    view_ = widget_->SetContentsView(std::make_unique<PopupBnplFootnoteView>(
        controller_.GetWeakPtr(), a11y_selection_delegate_,
        base::BindRepeating([](const std::u16string&, bool) {})));
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
  testing::NiceMock<MockAccessibilitySelectionDelegate>
      a11y_selection_delegate_;

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PopupBnplFootnoteView> view_ = nullptr;
};

TEST_F(PopupBnplFootnoteViewTest, AccessibleProperties) {
  ShowView();
  ui::AXNodeData node_data;

  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(ax::mojom::Role::kGroup, node_data.role);
}

}  // namespace autofill
