// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/overflow_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"

class OverflowViewTest : public testing::Test {
 public:
  OverflowViewTest() = default;
  ~OverflowViewTest() override = default;

  void SetUp() override {
    parent_view_ = std::make_unique<views::View>();
    parent_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    parent_view_->SetSize(kDefaultParentSize);
  }

  void TearDown() override {
    parent_view_.reset();
    overflow_view_ = nullptr;
  }

  std::unique_ptr<views::StaticSizedView> CreateIndicator(
      const gfx::Size& indicator_minimum_size,
      const gfx::Size& indicator_preferred_size) {
    auto indicator_view =
        std::make_unique<views::StaticSizedView>(indicator_preferred_size);
    indicator_view->set_minimum_size(indicator_minimum_size);
    return indicator_view;
  }

  void InitWithPrefix(const gfx::Size& primary_minimum_size,
                      const gfx::Size& primary_preferred_size,
                      const gfx::Size& prefix_indicator_minimum_size,
                      const gfx::Size& prefix_indicator_preferred_size) {
    auto primary_view =
        std::make_unique<views::StaticSizedView>(primary_preferred_size);
    primary_view->set_minimum_size(primary_minimum_size);
    primary_view_ = primary_view.get();
    auto prefix_indicator_view = CreateIndicator(
        prefix_indicator_minimum_size, prefix_indicator_preferred_size);
    prefix_indicator_view_ = prefix_indicator_view.get();
    overflow_view_ = parent_view_->AddChildView(std::make_unique<OverflowView>(
        std::move(primary_view), std::move(prefix_indicator_view), nullptr));
  }

  void InitWithPostfix(const gfx::Size& primary_minimum_size,
                       const gfx::Size& primary_preferred_size,
                       const gfx::Size& postfix_indicator_minimum_size,
                       const gfx::Size& postfix_indicator_preferred_size) {
    auto primary_view =
        std::make_unique<views::StaticSizedView>(primary_preferred_size);
    primary_view->set_minimum_size(primary_minimum_size);
    primary_view_ = primary_view.get();
    auto postfix_indicator_view = CreateIndicator(
        postfix_indicator_minimum_size, postfix_indicator_preferred_size);
    postfix_indicator_view_ = postfix_indicator_view.get();
    overflow_view_ = parent_view_->AddChildView(std::make_unique<OverflowView>(
        std::move(primary_view), std::move(postfix_indicator_view)));
  }

  void InitWithPrefixAndPostfix(
      const gfx::Size& primary_minimum_size,
      const gfx::Size& primary_preferred_size,
      const gfx::Size& prefix_indicator_minimum_size,
      const gfx::Size& prefix_indicator_preferred_size,
      const gfx::Size& postfix_indicator_minimum_size,
      const gfx::Size& postfix_indicator_preferred_size) {
    auto primary_view =
        std::make_unique<views::StaticSizedView>(primary_preferred_size);
    primary_view->set_minimum_size(primary_minimum_size);
    primary_view_ = primary_view.get();

    auto prefix_indicator_view = CreateIndicator(
        prefix_indicator_minimum_size, prefix_indicator_preferred_size);
    prefix_indicator_view_ = prefix_indicator_view.get();
    auto postfix_indicator_view = CreateIndicator(
        postfix_indicator_minimum_size, postfix_indicator_preferred_size);
    postfix_indicator_view_ = postfix_indicator_view.get();

    overflow_view_ = parent_view_->AddChildView(std::make_unique<OverflowView>(
        std::move(primary_view), std::move(prefix_indicator_view),
        std::move(postfix_indicator_view)));
  }

 protected:
  static int InterpolateByTens(int minimum,
                               int preferred,
                               views::SizeBound bound) {
    if (!bound.is_bounded())
      return preferred;
    if (bound.value() <= minimum)
      return minimum;
    if (bound.value() >= preferred)
      return preferred;
    return minimum + 10 * ((bound.value() - minimum) / 10);
  }

  // Flex rule where height and width step by 10s up from minimum to preferred
  // size.
  static gfx::Size StepwiseFlexRule(const views::View* view,
                                    const views::SizeBounds& bounds) {
    const gfx::Size preferred = view->GetPreferredSize(bounds);
    const gfx::Size minimum = view->GetMinimumSize();
    return gfx::Size(
        InterpolateByTens(minimum.width(), preferred.width(), bounds.width()),
        InterpolateByTens(minimum.height(), preferred.height(),
                          bounds.height()));
  }

  // Flex rule where the vertical axis contracts from preferred to minimum size
  // by the same percentage as the horizontal axis is shrunk between preferred
  // and minimum size. So for instance, if the horizontal axis is constrained
  // by 20 DIPs and the difference between minimum and preferred is 80 DIPs,
  // then the vertical axis will be 75% of the way between minimum and preferred
  // size.
  static gfx::Size ProportionalFlexRule(const views::View* view,
                                        const views::SizeBounds& bounds) {
    const gfx::Size preferred = view->GetPreferredSize(bounds);
    const gfx::Size minimum = view->GetMinimumSize();
    const int width =
        std::max(minimum.width(), bounds.width().min_of(preferred.width()));
    DCHECK_GT(preferred.width(), minimum.width());
    double ratio = static_cast<double>(width - minimum.width()) /
                   (preferred.width() - minimum.width());
    const int height = bounds.height().min_of(
        minimum.height() +
        base::ClampRound(ratio * (preferred.height() - minimum.height())));
    return gfx::Size(width, height);
  }

  static constexpr gfx::Size kDefaultParentSize{100, 70};
  static constexpr gfx::Size kPreferredSize{120, 80};
  static constexpr gfx::Size kMinimumSize{40, 20};
  static constexpr gfx::Size kPreferredSize2{55, 50};
  static constexpr gfx::Size kMinimumSize2{25, 30};
  std::unique_ptr<views::View> parent_view_;
  raw_ptr<OverflowView, DanglingUntriaged> overflow_view_ = nullptr;
  raw_ptr<views::StaticSizedView, DanglingUntriaged> primary_view_ = nullptr;
  raw_ptr<views::StaticSizedView, DanglingUntriaged> prefix_indicator_view_ =
      nullptr;
  raw_ptr<views::StaticSizedView, DanglingUntriaged> postfix_indicator_view_ =
      nullptr;
};

constexpr gfx::Size OverflowViewTest::kDefaultParentSize;
constexpr gfx::Size OverflowViewTest::kPreferredSize;
constexpr gfx::Size OverflowViewTest::kMinimumSize;
constexpr gfx::Size OverflowViewTest::kPreferredSize2;
constexpr gfx::Size OverflowViewTest::kMinimumSize2;

TEST_F(OverflowViewTest, SizesNoFlexRules) {
  InitWithPostfix(kMinimumSize, kPreferredSize, kMinimumSize2, kPreferredSize2);
  const gfx::Size expected_min(
      std::min(kMinimumSize2.width(), kMinimumSize.width()),
      std::max(kMinimumSize.height(), kMinimumSize2.height()));
  EXPECT_EQ(expected_min, overflow_view_->GetMinimumSize());
  EXPECT_EQ(kPreferredSize, overflow_view_->GetPreferredSize());
}

TEST_F(OverflowViewTest, SizesNoFlexRulesIndicatorIsLarger) {
  InitWithPostfix(kMinimumSize2, kPreferredSize2, kMinimumSize, kPreferredSize);
  const gfx::Size expected_min(
      std::min(kMinimumSize2.width(), kMinimumSize.width()),
      std::max(kMinimumSize.height(), kMinimumSize2.height()));
  EXPECT_EQ(expected_min, overflow_view_->GetMinimumSize());
  EXPECT_EQ(kPreferredSize2, overflow_view_->GetPreferredSize());
}

TEST_F(OverflowViewTest, SizesNoFlexRulesVertical) {
  InitWithPostfix(kMinimumSize, kPreferredSize, kMinimumSize2, kPreferredSize2);
  overflow_view_->SetOrientation(views::LayoutOrientation::kVertical);
  const gfx::Size expected_min(
      std::max(kMinimumSize.width(), kMinimumSize2.width()),
      std::min(kMinimumSize2.height(), kMinimumSize.height()));
  EXPECT_EQ(expected_min, overflow_view_->GetMinimumSize());
  EXPECT_EQ(kPreferredSize, overflow_view_->GetPreferredSize());
}

TEST_F(OverflowViewTest, SizesNoFlexRulesIndicatorIsLargerVertical) {
  InitWithPostfix(kMinimumSize2, kPreferredSize2, kMinimumSize, kPreferredSize);
  overflow_view_->SetOrientation(views::LayoutOrientation::kVertical);
  const gfx::Size expected_min(
      std::max(kMinimumSize.width(), kMinimumSize2.width()),
      std::min(kMinimumSize.height(), kMinimumSize2.height()));
  EXPECT_EQ(expected_min, overflow_view_->GetMinimumSize());
  EXPECT_EQ(kPreferredSize2, overflow_view_->GetPreferredSize());
}

class OverflowViewLayoutTest : public OverflowViewTest {
 public:
  OverflowViewLayoutTest() = default;
  ~OverflowViewLayoutTest() override = default;

  void SetUp() override { OverflowViewTest::SetUp(); }

  void Resize(gfx::Size size) {
    parent_view_->SetSize(size);
    views::test::RunScheduledLayout(parent_view_.get());
  }

  void SizeToPreferredSize() { parent_view_->SizeToPreferredSize(); }

  gfx::Rect primary_bounds() const { return primary_view_->bounds(); }
  gfx::Rect indicator_bounds() const {
    return postfix_indicator_view_->bounds();
  }
  bool primary_visible() const { return primary_view_->GetVisible(); }
  bool indicator_visible() const {
    return postfix_indicator_view_->GetVisible();
  }

 protected:
  static constexpr gfx::Size kPrimaryMinimumSize{80, 20};
  static constexpr gfx::Size kPrimaryPreferredSize{160, 30};
  static constexpr gfx::Size kIndicatorMinimumSize{16, 16};
  static constexpr gfx::Size kIndicatorPreferredSize{32, 32};

  static gfx::Size Transpose(const gfx::Size size) {
    return gfx::Size(size.height(), size.width());
  }

  static gfx::Size TransposingFlexRule(const views::View* view,
                                       const views::SizeBounds& size_bounds) {
    const gfx::Size preferred = Transpose(view->GetPreferredSize(size_bounds));
    const gfx::Size minimum = Transpose(view->GetMinimumSize());
    int height;
    int width;
    if (size_bounds.height().is_bounded()) {
      height = std::max(minimum.height(),
                        size_bounds.height().min_of(preferred.height()));
    } else {
      height = preferred.height();
    }
    if (size_bounds.width().is_bounded()) {
      width = std::max(minimum.width(),
                       size_bounds.width().min_of(preferred.width()));
    } else {
      width = preferred.width();
    }
    return gfx::Size(width, height);
  }
};

constexpr gfx::Size OverflowViewLayoutTest::kPrimaryMinimumSize;
constexpr gfx::Size OverflowViewLayoutTest::kPrimaryPreferredSize;
constexpr gfx::Size OverflowViewLayoutTest::kIndicatorMinimumSize;
constexpr gfx::Size OverflowViewLayoutTest::kIndicatorPreferredSize;

TEST_F(OverflowViewLayoutTest, SizeToPreferredSizeIndicatorSmallerThanPrimary) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  postfix_indicator_view_->SetPreferredSize(kIndicatorMinimumSize);
  SizeToPreferredSize();
  EXPECT_EQ(gfx::Rect(kPrimaryPreferredSize), primary_bounds());
  EXPECT_FALSE(indicator_visible());
}

TEST_F(OverflowViewLayoutTest, SizeToPreferredSizeIndicatorLargerThanPrimary) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  SizeToPreferredSize();
  gfx::Size expected = kPrimaryPreferredSize;
  EXPECT_EQ(gfx::Rect(expected), primary_bounds());
  EXPECT_FALSE(indicator_visible());
}

TEST_F(OverflowViewLayoutTest, ScaleToMinimum) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  // Since default cross-axis alignment is stretch, the view should fill the
  // space even if it's larger than the preferred size.
  gfx::Size size = kPrimaryPreferredSize;
  size.Enlarge(10, 10);
  Resize(size);
  EXPECT_EQ(gfx::Rect(kPrimaryPreferredSize.width(), size.height()),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  // Default behavior is to scale down smoothly between preferred and minimum
  // size.
  size = kPrimaryPreferredSize;
  size.Enlarge(-10, -10);
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  size = kPrimaryMinimumSize;
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  // Below minimum size, the stretch alignment means we'll compress.
  size.Enlarge(0, -5);
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());
  EXPECT_FALSE(indicator_visible());
}

TEST_F(OverflowViewLayoutTest, Alignment) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  gfx::Size size = kPrimaryPreferredSize;
  size.Enlarge(0, 10);
  Resize(size);

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 0), kPrimaryPreferredSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 5), kPrimaryPreferredSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 10), kPrimaryPreferredSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  size = kPrimaryMinimumSize;
  size.Enlarge(0, -10);
  Resize(size);

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 0), kPrimaryMinimumSize), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, -5), kPrimaryMinimumSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, -10), kPrimaryMinimumSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());
}

TEST_F(OverflowViewLayoutTest, ScaleToMinimumVertical) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  overflow_view_->SetOrientation(views::LayoutOrientation::kVertical);

  gfx::Size size = kPrimaryPreferredSize;
  size.Enlarge(10, 10);
  Resize(size);
  EXPECT_EQ(gfx::Rect(size.width(), kPrimaryPreferredSize.height()),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  // Default behavior is to scale down smoothly between preferred and minimum
  // size.
  size = kPrimaryPreferredSize;
  size.Enlarge(-10, -10);
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  size = kPrimaryMinimumSize;
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  // Below minimum size, the stretch alignment means we'll compress.
  size.Enlarge(-5, 0);
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());
  EXPECT_FALSE(indicator_visible());
}

TEST_F(OverflowViewLayoutTest, AlignmentVertical) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  overflow_view_->SetOrientation(views::LayoutOrientation::kVertical);

  gfx::Size size = kPrimaryPreferredSize;
  size.Enlarge(10, 0);
  Resize(size);

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(), kPrimaryPreferredSize), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(5, 0), kPrimaryPreferredSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(10, 0), kPrimaryPreferredSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  size = kPrimaryMinimumSize;
  size.Enlarge(-10, 0);
  Resize(size);

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 0), kPrimaryMinimumSize), primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(-5, 0), kPrimaryMinimumSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());

  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);
  views::test::RunScheduledLayout(parent_view_.get());
  EXPECT_EQ(gfx::Rect(gfx::Point(-10, 0), kPrimaryMinimumSize),
            primary_bounds());
  EXPECT_FALSE(indicator_visible());
}

TEST_F(OverflowViewLayoutTest, PrimaryOnlyRespectsFlexRule) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  primary_view_->SetProperty(views::kFlexBehaviorKey,
                             views::FlexSpecification(base::BindRepeating(
                                 &OverflowViewTest::StepwiseFlexRule)));
  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // Since default cross-axis alignment is stretch, the view should fill the
  // space even if it's larger than the preferred size.
  gfx::Size size = kPrimaryPreferredSize;
  size.Enlarge(7, 7);
  Resize(size);
  EXPECT_EQ(gfx::Rect(kPrimaryPreferredSize), primary_bounds());

  // At intermediate sizes, this flex rule steps down by multiples of 10, to the
  // next multiple smaller than the available space.
  size = kPrimaryPreferredSize;
  size.Enlarge(-7, -7);
  Resize(size);
  gfx::Size expected = kPrimaryPreferredSize;
  expected.Enlarge(-10, -10);
  EXPECT_EQ(gfx::Rect(expected), primary_bounds());

  // The height bottoms out against the minimum size first.
  size = kPrimaryPreferredSize;
  size.Enlarge(-13, -13);
  Resize(size);
  EXPECT_EQ(gfx::Rect(kPrimaryPreferredSize.width() - 20,
                      kPrimaryMinimumSize.height()),
            primary_bounds());

  size = kPrimaryMinimumSize;
  Resize(size);
  EXPECT_EQ(gfx::Rect(size), primary_bounds());

  // Below minimum vertical size we won't compress the primary view since we're
  // not stretching it.
  size.Enlarge(0, -5);
  Resize(size);
  EXPECT_EQ(gfx::Rect(kPrimaryMinimumSize), primary_bounds());
}

TEST_F(OverflowViewLayoutTest, HorizontalOverflow) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // The primary view should start at the preferred size and scale down until it
  // hits the minimum size.
  gfx::Size size = kPrimaryPreferredSize;
  size.Enlarge(10, 0);
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(kPrimaryPreferredSize), primary_bounds());

  size = kPrimaryPreferredSize;
  size.Enlarge(-10, 0);
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(size), primary_bounds());

  size = kPrimaryMinimumSize;
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(size), primary_bounds());

  // Below minimum size, the indicator will be displayed.
  size.Enlarge(-5, 0);
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_TRUE(postfix_indicator_view_->GetVisible());
  const gfx::Rect expected_indicator{
      size.width() - kIndicatorPreferredSize.width(), 0,
      kIndicatorPreferredSize.width(), size.height()};
  const gfx::Rect expected_primary(
      gfx::Size(expected_indicator.x(), size.height()));
  EXPECT_EQ(expected_indicator, indicator_bounds());
  EXPECT_EQ(expected_primary, primary_bounds());

  // If there is only enough room to show the indicator, then the primary view
  // loses visibility.
  size = kIndicatorMinimumSize;
  Resize(size);
  EXPECT_FALSE(primary_view_->GetVisible());
  EXPECT_TRUE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(size), indicator_bounds());
}

TEST_F(OverflowViewLayoutTest, VerticalOverflow) {
  InitWithPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                  kIndicatorMinimumSize, kIndicatorPreferredSize);
  overflow_view_->SetOrientation(views::LayoutOrientation::kVertical);
  overflow_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  const views::FlexRule flex_rule =
      base::BindRepeating(&OverflowViewLayoutTest::TransposingFlexRule);
  primary_view_->SetProperty(views::kFlexBehaviorKey,
                             views::FlexSpecification(flex_rule));
  postfix_indicator_view_->SetProperty(views::kFlexBehaviorKey,
                                       views::FlexSpecification(flex_rule));

  const gfx::Size primary_preferred = Transpose(kPrimaryPreferredSize);
  const gfx::Size primary_minimum = Transpose(kPrimaryMinimumSize);
  const gfx::Size indicator_preferred = Transpose(kIndicatorPreferredSize);
  const gfx::Size indicator_minimum = Transpose(kIndicatorMinimumSize);

  // The primary view should start at the preferred size and scale down until it
  // hits the minimum size.
  gfx::Size size = primary_preferred;
  size.Enlarge(0, 10);
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(primary_preferred), primary_bounds());

  size = primary_preferred;
  size.Enlarge(0, -10);
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(size), primary_bounds());

  size = primary_minimum;
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(size), primary_bounds());

  // Below minimum size, the indicator will be displayed.
  size.Enlarge(0, -5);
  Resize(size);
  EXPECT_TRUE(primary_view_->GetVisible());
  EXPECT_TRUE(postfix_indicator_view_->GetVisible());
  const gfx::Rect expected_indicator{
      0, size.height() - indicator_preferred.height(), size.width(),
      indicator_preferred.height()};
  const gfx::Rect expected_primary(
      gfx::Size(size.width(), expected_indicator.y()));
  EXPECT_EQ(expected_indicator, indicator_bounds());
  EXPECT_EQ(expected_primary, primary_bounds());

  size = indicator_minimum;
  Resize(size);
  EXPECT_FALSE(primary_view_->GetVisible());
  EXPECT_TRUE(postfix_indicator_view_->GetVisible());
  EXPECT_EQ(gfx::Rect(size), indicator_bounds());
}

TEST_F(OverflowViewLayoutTest, PrefixAndPostfixDisplayed) {
  InitWithPrefixAndPostfix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                           kIndicatorMinimumSize, kIndicatorPreferredSize,
                           kIndicatorMinimumSize, kIndicatorPreferredSize);

  gfx::Size size = kPrimaryMinimumSize;
  size.Enlarge(-5, 0);
  Resize(size);

  EXPECT_TRUE(prefix_indicator_view_ && prefix_indicator_view_->GetVisible());
  EXPECT_TRUE(postfix_indicator_view_ && postfix_indicator_view_->GetVisible());
}

TEST_F(OverflowViewLayoutTest, PrefixOnlyDisplayed) {
  InitWithPrefix(kPrimaryMinimumSize, kPrimaryPreferredSize,
                 kIndicatorMinimumSize, kIndicatorPreferredSize);

  gfx::Size size = kPrimaryMinimumSize;
  size.Enlarge(-5, 0);
  Resize(size);

  EXPECT_TRUE(prefix_indicator_view_ && prefix_indicator_view_->GetVisible());
  EXPECT_FALSE(postfix_indicator_view_);
}
