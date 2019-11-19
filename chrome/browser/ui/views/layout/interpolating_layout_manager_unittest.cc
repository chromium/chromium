// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout/interpolating_layout_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace {

class TestLayout : public views::LayoutManagerBase {
 public:
  explicit TestLayout(int size = 0) : size_(size) {}

  int num_layouts_generated() const { return num_layouts_generated_; }

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    ++num_layouts_generated_;
    views::ProposedLayout layout;
    int x = size_;
    for (auto it = host_view()->children().begin();
         it != host_view()->children().end(); ++it) {
      if (!IsChildIncludedInLayout(*it))
        continue;
      views::ChildLayout child_layout;
      child_layout.child_view = *it;
      child_layout.visible = true;
      child_layout.bounds = gfx::Rect(x, 1, size_, size_);
      layout.child_layouts.push_back(child_layout);
      x += size_ + 1;
    }
    layout.host_size = {x, 2 + size_};
    return layout;
  }

 private:
  const int size_;
  mutable int num_layouts_generated_ = 0;
};

void CompareProposedLayouts(const views::ProposedLayout& left,
                            const views::ProposedLayout& right) {
  EXPECT_EQ(left.host_size, right.host_size);
  EXPECT_EQ(left.child_layouts.size(), right.child_layouts.size());
  for (auto left_it = left.child_layouts.begin(),
            right_it = right.child_layouts.begin();
       left_it != left.child_layouts.end() &&
       right_it != right.child_layouts.end();
       ++left_it, ++right_it) {
    EXPECT_EQ(left_it->child_view, right_it->child_view);
    EXPECT_EQ(left_it->visible, right_it->visible);
    if (left_it->visible)
      EXPECT_EQ(left_it->bounds, right_it->bounds);
  }
}

}  // anonymous namespace

class InterpolatingLayoutManagerTest : public testing::Test {
 public:
  void SetUp() override {
    host_view_ = std::make_unique<views::View>();
    layout_manager_ = host_view_->SetLayoutManager(
        std::make_unique<InterpolatingLayoutManager>());
  }

  InterpolatingLayoutManager* layout_manager() { return layout_manager_; }
  views::View* host_view() { return host_view_.get(); }

 private:
  InterpolatingLayoutManager* layout_manager_ = nullptr;
  std::unique_ptr<views::View> host_view_;
};

TEST_F(InterpolatingLayoutManagerTest, AddLayout) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({0, 0});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, AddLayout_CheckZeroAndUnbounded) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {5, 0});
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(0, second_layout->num_layouts_generated());
  layout_manager()->GetPreferredSize(host_view());
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  layout_manager()->GetMinimumSize(host_view());
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout_HardBoundary) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {5, 0});
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(0, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({5, 2});
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({4, 2});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout_SoftBoudnary) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(0, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({5, 2});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({4, 2});
  EXPECT_EQ(2, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({6, 6});
  EXPECT_EQ(2, first_layout->num_layouts_generated());
  EXPECT_EQ(2, second_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout_MultipleLayouts) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  TestLayout* const third_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {6, 2});
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(0, second_layout->num_layouts_generated());
  EXPECT_EQ(0, third_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({5, 2});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  EXPECT_EQ(0, third_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({6, 3});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(2, second_layout->num_layouts_generated());
  EXPECT_EQ(0, third_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({7, 6});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(3, second_layout->num_layouts_generated());
  EXPECT_EQ(1, third_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({20, 3});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(3, second_layout->num_layouts_generated());
  EXPECT_EQ(2, third_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, InvalidateLayout) {
  static const gfx::Size kLayoutSize(5, 5);

  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  host_view()->SetSize(kLayoutSize);
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  host_view()->Layout();
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  host_view()->InvalidateLayout();
  host_view()->Layout();
  EXPECT_EQ(2, first_layout->num_layouts_generated());
  EXPECT_EQ(2, second_layout->num_layouts_generated());
  host_view()->Layout();
  EXPECT_EQ(2, first_layout->num_layouts_generated());
  EXPECT_EQ(2, second_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, SetOrientation) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>());
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  layout_manager()->SetOrientation(views::LayoutOrientation::kVertical);
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(0, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({2, 6});
  EXPECT_EQ(0, first_layout->num_layouts_generated());
  EXPECT_EQ(1, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({3, 5});
  EXPECT_EQ(1, first_layout->num_layouts_generated());
  EXPECT_EQ(2, second_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout({10, 3});
  EXPECT_EQ(2, first_layout->num_layouts_generated());
  EXPECT_EQ(2, second_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetMinimumSize) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(5));
  layout_manager()->AddLayout(std::make_unique<TestLayout>(10), {5, 5});

  // Minimum size should be equal to the default layout.
  EXPECT_EQ(first_layout->GetMinimumSize(host_view()),
            layout_manager()->GetMinimumSize(host_view()));
}

TEST_F(InterpolatingLayoutManagerTest, GetPreferredSize_NoDefaultLayout) {
  layout_manager()->AddLayout(std::make_unique<TestLayout>(5));
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(10), {5, 5});

  // Preferred size should be equal to the largest layout.
  EXPECT_EQ(second_layout->GetPreferredSize(host_view()),
            layout_manager()->GetPreferredSize(host_view()));
}

TEST_F(InterpolatingLayoutManagerTest, GetPreferredSize_UsesDefaultLayout) {
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(5));
  layout_manager()->AddLayout(std::make_unique<TestLayout>(10), {5, 5});
  layout_manager()->SetDefaultLayout(first_layout);

  // Preferred size should be equal to the largest layout.
  EXPECT_EQ(first_layout->GetPreferredSize(host_view()),
            layout_manager()->GetPreferredSize(host_view()));
}

TEST_F(InterpolatingLayoutManagerTest, GetPreferredHeightForWidth_Vertical) {
  layout_manager()->SetOrientation(views::LayoutOrientation::kVertical);
  layout_manager()->AddLayout(std::make_unique<TestLayout>(5));
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(10), {5, 5});

  // Vertical means preferred height for width applies to largest layout.
  EXPECT_EQ(second_layout->GetPreferredHeightForWidth(host_view(), 7),
            layout_manager()->GetPreferredHeightForWidth(host_view(), 7));
  EXPECT_EQ(second_layout->GetPreferredHeightForWidth(host_view(), 3),
            layout_manager()->GetPreferredHeightForWidth(host_view(), 3));
  EXPECT_EQ(second_layout->GetPreferredHeightForWidth(host_view(), 10),
            layout_manager()->GetPreferredHeightForWidth(host_view(), 10));
}

TEST_F(InterpolatingLayoutManagerTest, GetPreferredHeightForWidth_Horizontal) {
  layout_manager()->SetOrientation(views::LayoutOrientation::kHorizontal);
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(5));
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(10), {5, 5});

  // Horizontal means preferred height for width is interpolated.
  // Note that the layout doesn't actually flex height with varying width, so we
  // can use constant reference values.
  const int default_height =
      first_layout->GetPreferredHeightForWidth(host_view(), 7);
  const int other_height =
      second_layout->GetPreferredHeightForWidth(host_view(), 7);
  EXPECT_EQ(default_height,
            layout_manager()->GetPreferredHeightForWidth(host_view(), 5));
  EXPECT_EQ(other_height,
            layout_manager()->GetPreferredHeightForWidth(host_view(), 10));
  EXPECT_EQ(int{default_height * 0.4f + other_height * 0.6f},
            layout_manager()->GetPreferredHeightForWidth(host_view(), 8));
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout) {
  views::View* const child_view =
      host_view()->AddChildView(std::make_unique<views::View>());
  TestLayout* const first_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(5));
  TestLayout* const second_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(10), {5, 6});

  constexpr gfx::Size kSmallSize{5, 10};
  constexpr gfx::Size kLargeSize{11, 10};
  constexpr gfx::Size kOneThirdSize{7, 10};
  constexpr gfx::Size kOneHalfSize{8, 10};
  const views::ProposedLayout expected_default =
      first_layout->GetProposedLayout(kSmallSize);
  const views::ProposedLayout expected_other =
      second_layout->GetProposedLayout(kLargeSize);

  CompareProposedLayouts(expected_default,
                         layout_manager()->GetProposedLayout(kSmallSize));
  CompareProposedLayouts(expected_other,
                         layout_manager()->GetProposedLayout(kLargeSize));

  views::ProposedLayout actual =
      layout_manager()->GetProposedLayout(kOneThirdSize);
  EXPECT_EQ(gfx::Tween::SizeValueBetween(0.3333, expected_default.host_size,
                                         expected_other.host_size),
            actual.host_size);
  ASSERT_EQ(1U, actual.child_layouts.size());
  EXPECT_EQ(child_view, actual.child_layouts[0].child_view);
  EXPECT_TRUE(actual.child_layouts[0].visible);
  EXPECT_EQ(gfx::Tween::RectValueBetween(
                0.3333, expected_default.child_layouts[0].bounds,
                expected_other.child_layouts[0].bounds),
            actual.child_layouts[0].bounds);

  actual = layout_manager()->GetProposedLayout(kOneHalfSize);
  EXPECT_EQ(gfx::Tween::SizeValueBetween(0.5, expected_default.host_size,
                                         expected_other.host_size),
            actual.host_size);
  ASSERT_EQ(1U, actual.child_layouts.size());
  EXPECT_EQ(child_view, actual.child_layouts[0].child_view);
  EXPECT_TRUE(actual.child_layouts[0].visible);
  EXPECT_EQ(gfx::Tween::RectValueBetween(
                0.5, expected_default.child_layouts[0].bounds,
                expected_other.child_layouts[0].bounds),
            actual.child_layouts[0].bounds);
}
