// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/new_badge_label.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"

namespace user_education {

class NewBadgeLabelTest : public views::ViewsTestBase {
 public:
  NewBadgeLabelTest() = default;
  ~NewBadgeLabelTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    constexpr gfx::Size kNewBadgeLabelTestWidgetSize(300, 300);
    params.bounds = gfx::Rect(gfx::Point(), kNewBadgeLabelTestWidgetSize);
    widget_->Init(std::move(params));
    contents_ = widget_->SetContentsView(std::make_unique<views::View>());
    contents_->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

    control_label_ = contents_->AddChildView(
        std::make_unique<views::Label>(u"test", views::style::CONTEXT_LABEL));
    new_badge_label_ = contents_->AddChildView(
        std::make_unique<NewBadgeLabel>(u"test", views::style::CONTEXT_LABEL));
  }

  void TearDown() override {
    widget_.reset();
    contents_ = nullptr;
    control_label_ = nullptr;
    new_badge_label_ = nullptr;
    ViewsTestBase::TearDown();
  }

  views::Widget* widget() const { return widget_.get(); }
  views::Label* control_label() const { return control_label_; }
  NewBadgeLabel* new_badge_label() const { return new_badge_label_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View, DanglingUntriaged> contents_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> control_label_ = nullptr;
  raw_ptr<NewBadgeLabel, DanglingUntriaged> new_badge_label_ = nullptr;
};

TEST_F(NewBadgeLabelTest, NoBadgeReportsSameSizes) {
  new_badge_label()->SetDisplayNewBadge(false);
  const gfx::Size preferred_size = control_label()->GetPreferredSize();
  EXPECT_EQ(preferred_size, new_badge_label()->GetPreferredSize());
  EXPECT_EQ(control_label()->GetMinimumSize(),
            new_badge_label()->GetMinimumSize());
  EXPECT_EQ(control_label()->GetHeightForWidth(preferred_size.width()),
            new_badge_label()->GetHeightForWidth(preferred_size.width()));
  EXPECT_EQ(control_label()->GetHeightForWidth(preferred_size.width() / 2),
            new_badge_label()->GetHeightForWidth(preferred_size.width() / 2));
}

TEST_F(NewBadgeLabelTest, NoBadgeLayoutsAreTheSame) {
  new_badge_label()->SetDisplayNewBadge(false);
  widget()->Show();
  widget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(control_label()->size(), new_badge_label()->size());
  EXPECT_EQ(control_label()->GetInsets(), new_badge_label()->GetInsets());
  EXPECT_EQ(nullptr,
            new_badge_label()->GetProperty(views::kInternalPaddingKey));
}

TEST_F(NewBadgeLabelTest, WithBadgeReportsDifferentSizes) {
  // Width should be less for the control than for the new badge label.
  EXPECT_LT(control_label()->GetPreferredSize().width(),
            new_badge_label()->GetPreferredSize().width());
  EXPECT_LT(control_label()->GetMinimumSize().width(),
            new_badge_label()->GetMinimumSize().width());
  // Height should be less or the same for the control than for the new badge
  // label.
  EXPECT_LE(control_label()->GetPreferredSize().height(),
            new_badge_label()->GetPreferredSize().height());
  EXPECT_LE(control_label()->GetMinimumSize().height(),
            new_badge_label()->GetMinimumSize().height());
}

TEST_F(NewBadgeLabelTest, WithBadgeLayoutsAreDifferent) {
  widget()->Show();
  widget()->LayoutRootViewIfNecessary();
  // Width should be less for the control than for the new badge label.
  EXPECT_LT(control_label()->size().width(), new_badge_label()->size().width());
  // Height should be less or the same for the control than for the new badge
  // label.
  EXPECT_LE(control_label()->size().height(),
            new_badge_label()->size().height());

  EXPECT_NE(control_label()->GetInsets(), new_badge_label()->GetInsets());
  EXPECT_NE(nullptr,
            new_badge_label()->GetProperty(views::kInternalPaddingKey));
}

TEST_F(NewBadgeLabelTest, SetDisplayNewBadgeCorrectlyAffectsCalculations) {
  EXPECT_TRUE(new_badge_label()->GetDisplayNewBadge());

  // Default is true. Setting it again should have no effect.
  new_badge_label()->SetDisplayNewBadge(true);
  EXPECT_TRUE(new_badge_label()->GetDisplayNewBadge());
  EXPECT_LT(control_label()->GetPreferredSize().width(),
            new_badge_label()->GetPreferredSize().width());

  // Toggle to false, observe correct behavior.
  new_badge_label()->SetDisplayNewBadge(false);
  EXPECT_FALSE(new_badge_label()->GetDisplayNewBadge());
  EXPECT_EQ(control_label()->GetPreferredSize().width(),
            new_badge_label()->GetPreferredSize().width());

  // Set to false again, no change.
  new_badge_label()->SetDisplayNewBadge(false);
  EXPECT_FALSE(new_badge_label()->GetDisplayNewBadge());
  EXPECT_EQ(control_label()->GetPreferredSize().width(),
            new_badge_label()->GetPreferredSize().width());

  // Set back to true and verify default behavior.
  new_badge_label()->SetDisplayNewBadge(true);
  EXPECT_TRUE(new_badge_label()->GetDisplayNewBadge());
  EXPECT_LT(control_label()->GetPreferredSize().width(),
            new_badge_label()->GetPreferredSize().width());
}

}  // namespace user_education
