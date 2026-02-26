// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_loading_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class PopupLoadingViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void ShowView(int expected_number_of_suggestions) {
    view_ = widget_->SetContentsView(
        std::make_unique<PopupLoadingView>(expected_number_of_suggestions));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupLoadingView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PopupLoadingView> view_ = nullptr;
};

TEST_F(PopupLoadingViewTest, AccessibleProperties) {
  ShowView(/*expected_number_of_suggestions=*/1);
  ui::AXNodeData node_data;

  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(ax::mojom::Role::kProgressIndicator, node_data.role);
}

TEST_F(PopupLoadingViewTest, HasThrobberChild) {
  ShowView(/*expected_number_of_suggestions=*/2);

  ASSERT_EQ(view().children().size(), 1u);
  views::View* box_layout_view = view().children()[0];
  ASSERT_EQ(box_layout_view->children().size(), 1u);
  EXPECT_EQ(box_layout_view->children()[0]->GetClassName(),
            std::string_view("Throbber"));
}

TEST_F(PopupLoadingViewTest, PreferredSizeMatchesInput) {
  const int kExpectedSuggestions = 3;
  ShowView(kExpectedSuggestions);

  Suggestion dummy_suggestion(SuggestionType::kBnplEntry);
  dummy_suggestion.main_text = Suggestion::Text(u"Dummy Issuer");
  dummy_suggestion.labels = {{Suggestion::Text(u"Pay in 4 installments")}};

  auto dummy_view = CreatePopupRowContentView(dummy_suggestion,
                                              /*show_new_badge=*/std::nullopt,
                                              FillingProduct::kCreditCard,
                                              /*filter_match=*/std::nullopt);
  gfx::Size single_suggestion_size = dummy_view->GetPreferredSize();

  gfx::Size expected_size(
      single_suggestion_size.width(),
      single_suggestion_size.height() * kExpectedSuggestions);
  EXPECT_EQ(view().GetPreferredSize(), expected_size);
}

}  // namespace autofill
