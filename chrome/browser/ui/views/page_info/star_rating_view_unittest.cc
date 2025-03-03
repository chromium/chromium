// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/star_rating_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/test/views_test_base.h"

class StarRatingViewTest : public views::ViewsTestBase {
 public:
  StarRatingViewTest() = default;

  StarRatingViewTest(const StarRatingViewTest&) = delete;
  StarRatingViewTest& operator=(const StarRatingViewTest&) = delete;

  // testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    view_ = std::make_unique<StarRatingView>();
  }

  StarRatingView* view() { return view_.get(); }

  bool IsFullStarIconAt(int index) {
    const ui::VectorIconModel& model =
        view()->GetVectorIconModelForIndexForTesting(index);
    return model.vector_icon()->name == vector_icons::kStarIcon.name &&
           model.color() == kColorStarRatingFullIcon;
  }

  bool IsHalfStarIconAt(int index) {
    const ui::VectorIconModel& model =
        view()->GetVectorIconModelForIndexForTesting(index);
    return model.vector_icon()->name == vector_icons::kStarHalfIcon.name &&
           model.color() == kColorStarRatingFullIcon;
  }

  bool IsEmptyStarIconAt(int index) {
    const ui::VectorIconModel& model =
        view()->GetVectorIconModelForIndexForTesting(index);
    return model.vector_icon()->name == vector_icons::kStarIcon.name &&
           model.color() == kColorStarRatingEmptyIcon;
  }

 private:
  std::unique_ptr<StarRatingView> view_;
};

TEST_F(StarRatingViewTest, ZeroStars) {
  view()->SetRating(0.0);
  EXPECT_EQ(view()->GetTextForTesting(), u"0.0");
  EXPECT_TRUE(IsEmptyStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(0.449);
  EXPECT_EQ(view()->GetTextForTesting(), u"0.4");
  EXPECT_TRUE(IsEmptyStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));
}

TEST_F(StarRatingViewTest, HalfStar) {
  view()->SetRating(0.5);
  EXPECT_EQ(view()->GetTextForTesting(), u"0.5");
  EXPECT_TRUE(IsHalfStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(0.499);
  EXPECT_EQ(view()->GetTextForTesting(), u"0.5");
  EXPECT_TRUE(IsHalfStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(0.949);
  EXPECT_EQ(view()->GetTextForTesting(), u"0.9");
  EXPECT_TRUE(IsHalfStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));
}

TEST_F(StarRatingViewTest, OneStar) {
  view()->SetRating(1.0);
  EXPECT_EQ(view()->GetTextForTesting(), u"1.0");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(0.999);
  EXPECT_EQ(view()->GetTextForTesting(), u"1.0");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(1.449);
  EXPECT_EQ(view()->GetTextForTesting(), u"1.4");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsEmptyStarIconAt(1));
  EXPECT_TRUE(IsEmptyStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));
}

TEST_F(StarRatingViewTest, FourAndHalfStars) {
  view()->SetRating(4.5);
  EXPECT_EQ(view()->GetTextForTesting(), u"4.5");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsFullStarIconAt(3));
  EXPECT_TRUE(IsHalfStarIconAt(4));

  view()->SetRating(4.949);
  EXPECT_EQ(view()->GetTextForTesting(), u"4.9");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsFullStarIconAt(3));
  EXPECT_TRUE(IsHalfStarIconAt(4));
}

TEST_F(StarRatingViewTest, ThreeStars) {
  view()->SetRating(3.0);
  EXPECT_EQ(view()->GetTextForTesting(), u"3.0");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(2.999);
  EXPECT_EQ(view()->GetTextForTesting(), u"3.0");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));

  view()->SetRating(3.449);
  EXPECT_EQ(view()->GetTextForTesting(), u"3.4");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsEmptyStarIconAt(3));
  EXPECT_TRUE(IsEmptyStarIconAt(4));
}

TEST_F(StarRatingViewTest, FiveStars) {
  view()->SetRating(5.0);
  EXPECT_EQ(view()->GetTextForTesting(), u"5.0");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsFullStarIconAt(3));
  EXPECT_TRUE(IsFullStarIconAt(4));

  view()->SetRating(4.999);
  EXPECT_EQ(view()->GetTextForTesting(), u"5.0");
  EXPECT_TRUE(IsFullStarIconAt(0));
  EXPECT_TRUE(IsFullStarIconAt(1));
  EXPECT_TRUE(IsFullStarIconAt(2));
  EXPECT_TRUE(IsFullStarIconAt(3));
  EXPECT_TRUE(IsFullStarIconAt(4));
}
