// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include <memory>
#include <string>

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_highlighter_views.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_utils.h"

namespace bubble_anchor_util {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButtonElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSeparatorElementId);

class BubbleAnchorUtilViewTest : public ChromeViewsTestBase {
 public:
  BubbleAnchorUtilViewTest() = default;

  BubbleAnchorUtilViewTest(const BubbleAnchorUtilViewTest&) = delete;
  BubbleAnchorUtilViewTest& operator=(const BubbleAnchorUtilViewTest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ui::ElementHighlighter::GetElementHighlighter()
        ->MaybeRegisterBackend<views::ElementHighlighterViews>();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    // Create a button (higlightable) and separator (not), and give them IDs.
    button_ = widget_->SetContentsView(std::make_unique<views::LabelButton>(
        views::Button::PressedCallback(), std::u16string()));
    button_->SetProperty(views::kElementIdentifierKey, kButtonElementId);
    button_->SetVisible(true);
    separator_ = button_->AddChildView(std::make_unique<views::Separator>());
    separator_->SetProperty(views::kElementIdentifierKey, kSeparatorElementId);
    separator_->SetVisible(true);
  }

  void TearDown() override {
    button_ = nullptr;
    separator_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::LabelButton> button_ = nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;
};

TEST_F(BubbleAnchorUtilViewTest, IsHighlightableView) {
  EXPECT_TRUE(IsHighlightable(button_));
  EXPECT_FALSE(IsHighlightable(separator_));

  // The BubbleAnchor variant is a bit strange since one can end up with a
  // nullptr_t null, or a Views* null; so make sure the latter works fine, too.
  views::View* null_view = nullptr;
  views::BubbleAnchor null_view_anchor = null_view;
  EXPECT_TRUE(std::holds_alternative<views::View*>(null_view_anchor));
  EXPECT_FALSE(IsHighlightable(null_view_anchor));
}

TEST_F(BubbleAnchorUtilViewTest, IsHighlightableElement) {
  ui::TrackedElement* button_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(button_);
  ASSERT_TRUE(button_element);
  ui::TrackedElement* separator_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(separator_);
  ASSERT_TRUE(separator_element);

  EXPECT_TRUE(IsHighlightable(button_element));
  EXPECT_FALSE(IsHighlightable(separator_element));

  // The BubbleAnchor variant is a bit strange since one can end up with a
  // nullptr_t null, or a ui::TrackedElement* null; so make sure the latter
  // works fine, too.
  ui::TrackedElement* null_element = nullptr;
  views::BubbleAnchor null_element_anchor = null_element;
  EXPECT_TRUE(std::holds_alternative<ui::TrackedElement*>(null_element_anchor));
  EXPECT_FALSE(IsHighlightable(null_element_anchor));
}

TEST_F(BubbleAnchorUtilViewTest, IsHighlightableNull) {
  EXPECT_FALSE(IsHighlightable(nullptr));
}

}  // namespace bubble_anchor_util
