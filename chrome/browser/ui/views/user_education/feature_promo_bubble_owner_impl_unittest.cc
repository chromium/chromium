// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner_impl.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FeaturePromoBubbleOwnerImplTest : public ChromeViewsTestBase {
 public:
  FeaturePromoBubbleOwnerImplTest() = default;
  ~FeaturePromoBubbleOwnerImplTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    anchor_widget_ = CreateTestWidget();
    anchor_widget_->Show();

    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_view_->SetPreferredSize(gfx::Size(100, 100));
    anchor_widget_->LayoutRootViewIfNecessary();
  }

  void TearDown() override {
    anchor_view_ = nullptr;
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  FeaturePromoBubbleView::CreateParams GetBubbleParams() {
    FeaturePromoBubbleView::CreateParams params;
    params.body_text = u"To X, do Y";
    params.anchor_view = anchor_view_;
    params.arrow = views::BubbleBorder::TOP_RIGHT;
    return params;
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::View> anchor_view_ = nullptr;

  FeaturePromoBubbleOwnerImpl bubble_owner_;
};

TEST_F(FeaturePromoBubbleOwnerImplTest, ShowsBubble) {
  EXPECT_FALSE(bubble_owner_.AnyBubbleIsShowing());
  EXPECT_EQ(nullptr, bubble_owner_.bubble_for_testing());

  auto bubble_id =
      bubble_owner_.ShowBubble(GetBubbleParams(), base::DoNothing());
  ASSERT_TRUE(bubble_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*bubble_id));
  EXPECT_TRUE(bubble_owner_.AnyBubbleIsShowing());
  EXPECT_NE(nullptr, bubble_owner_.bubble_for_testing());
}

TEST_F(FeaturePromoBubbleOwnerImplTest, DoesNotShowTwoBubbles) {
  auto bubble_id =
      bubble_owner_.ShowBubble(GetBubbleParams(), base::DoNothing());
  ASSERT_TRUE(bubble_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*bubble_id));
  FeaturePromoBubbleView* const first_bubble =
      bubble_owner_.bubble_for_testing();

  EXPECT_FALSE(bubble_owner_.ShowBubble(GetBubbleParams(), base::DoNothing()));
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*bubble_id));
  EXPECT_EQ(first_bubble, bubble_owner_.bubble_for_testing());
}

TEST_F(FeaturePromoBubbleOwnerImplTest, ClosesBubble) {
  base::MockOnceClosure close_callback;
  auto bubble_id =
      bubble_owner_.ShowBubble(GetBubbleParams(), close_callback.Get());
  ASSERT_TRUE(bubble_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*bubble_id));
  EXPECT_NE(nullptr, bubble_owner_.bubble_for_testing());

  EXPECT_CALL(close_callback, Run()).Times(1);
  bubble_owner_.CloseBubble(*bubble_id);
  EXPECT_FALSE(bubble_owner_.BubbleIsShowing(*bubble_id));
  EXPECT_FALSE(bubble_owner_.AnyBubbleIsShowing());
  EXPECT_EQ(nullptr, bubble_owner_.bubble_for_testing());
}

TEST_F(FeaturePromoBubbleOwnerImplTest, OpenSecondBubbleAfterClose) {
  auto first_id =
      bubble_owner_.ShowBubble(GetBubbleParams(), base::DoNothing());
  ASSERT_TRUE(first_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*first_id));
  EXPECT_NE(nullptr, bubble_owner_.bubble_for_testing());

  bubble_owner_.CloseBubble(*first_id);
  EXPECT_FALSE(bubble_owner_.BubbleIsShowing(*first_id));
  EXPECT_EQ(nullptr, bubble_owner_.bubble_for_testing());

  auto second_id =
      bubble_owner_.ShowBubble(GetBubbleParams(), base::DoNothing());
  ASSERT_TRUE(second_id);
  // The probability of this happening is astronomically low
  ASSERT_NE(*first_id, *second_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*second_id));
  EXPECT_FALSE(bubble_owner_.BubbleIsShowing(*first_id));
  EXPECT_NE(nullptr, bubble_owner_.bubble_for_testing());

  // Attempting to close the first bubble ID  again should do nothing.
  bubble_owner_.CloseBubble(*first_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*second_id));
  EXPECT_FALSE(bubble_owner_.BubbleIsShowing(*first_id));
  EXPECT_NE(nullptr, bubble_owner_.bubble_for_testing());
}

TEST_F(FeaturePromoBubbleOwnerImplTest, HandlesBubbleClosedByToolkit) {
  base::MockOnceClosure close_callback;
  auto bubble_id =
      bubble_owner_.ShowBubble(GetBubbleParams(), close_callback.Get());
  ASSERT_TRUE(bubble_id);
  EXPECT_TRUE(bubble_owner_.BubbleIsShowing(*bubble_id));
  FeaturePromoBubbleView* const bubble = bubble_owner_.bubble_for_testing();

  EXPECT_CALL(close_callback, Run());
  bubble->GetWidget()->Close();
  EXPECT_FALSE(bubble_owner_.BubbleIsShowing(*bubble_id));
  EXPECT_EQ(nullptr, bubble_owner_.bubble_for_testing());
}
