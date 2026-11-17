// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"

#include <optional>

#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/common/horizontal_tab_style_views.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Minimal TabStyleViewDelegate whose state can be set directly by tests.
class FakeTabStyleViewDelegate : public TabStyleViewDelegate {
 public:
  FakeTabStyleViewDelegate() = default;
  ~FakeTabStyleViewDelegate() override = default;

  void set_active(bool active) { active_ = active; }
  void set_selected(bool selected) { selected_ = selected; }
  void set_hovering(bool hovering) { hovering_ = hovering; }
  void set_pinned(bool pinned) { pinned_ = pinned; }
  void set_group(std::optional<tab_groups::TabGroupId> group) {
    group_ = group;
  }
  void set_hover_animation_value(float value) {
    hover_animation_value_ = value;
  }

  // TabStyleViewDelegate:
  const views::View* GetView() const override { return nullptr; }
  bool IsActive() const override { return active_; }
  bool IsSelected() const override { return selected_; }
  bool IsHovering() const override { return hovering_; }
  bool IsClosing() const override { return false; }
  bool IsDragging() const override { return false; }
  std::optional<tab_groups::TabGroupId> GetGroup() const override {
    return group_;
  }
  std::optional<SkColor> GetGroupColor() const override { return std::nullopt; }
  bool IsInFocusedGroup() const override { return false; }
  bool IsGroupCollapsed() const override { return false; }
  bool IsSplit() const override { return false; }
  std::optional<split_tabs::SplitTabId> GetSplit() const override {
    return std::nullopt;
  }
  bool IsLeftSplitTab() const override { return false; }
  bool IsRightSplitTab() const override { return false; }
  int GetTabCount() const override { return 1; }
  const TabStyleViewDelegate* GetAdjacentTab(bool leading) const override {
    return nullptr;
  }
  float GetHoverAnimationValue() const override {
    return hover_animation_value_;
  }
  float GetHoverOpacity() const override { return 0.0f; }
  bool IsHoverAnimationActive() const override {
    return hover_animation_value_ > 0.0f;
  }
  BrowserFrameView* GetBrowserFrameView() const override { return nullptr; }
  BrowserWindowInterface* GetBrowserWindowInterface() const override {
    return nullptr;
  }
  bool IsGlassFrame() const override { return false; }
  bool IsPinned() const override { return pinned_; }
  bool ShouldPaintTabBackgroundColor() const override { return true; }
  int GetStrokeThickness() const override { return 0; }
  GlowHoverController* GetHoverControllerForTesting() override {
    return nullptr;
  }

 private:
  bool active_ = false;
  bool selected_ = false;
  bool hovering_ = false;
  bool pinned_ = false;
  std::optional<tab_groups::TabGroupId> group_;
  float hover_animation_value_ = 0.0f;
};

constexpr int kTabCount = 3;

}  // namespace

class TabGroupStyleSeparatorTest : public testing::Test {
 public:
  TabGroupStyleSeparatorTest() {
    // Make sure the declutter state doesn't depend on the default feature
    // configuration.
    feature_list_.InitWithFeatures(
        {}, {features::kTabStripDeclutter, features::kDesktopGlowUp});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TabGroupStyleSeparatorTest, ExpandedGroupHasNoSeparators) {
  const TabStyle::SeparatorOpacities opacities =
      TabGroupStyle::GetSeparatorOpacities(/*is_collapsed=*/false,
                                           /*is_dragging=*/false, kTabCount,
                                           /*adjacent_tab=*/nullptr);
  EXPECT_EQ(opacities.left, 0.0f);
  EXPECT_EQ(opacities.right, 0.0f);
}

TEST_F(TabGroupStyleSeparatorTest, CollapsedGroupHasOnlyTrailingSeparator) {
  const TabStyle::SeparatorOpacities opacities =
      TabGroupStyle::GetSeparatorOpacities(/*is_collapsed=*/true,
                                           /*is_dragging=*/false, kTabCount,
                                           /*adjacent_tab=*/nullptr);
  EXPECT_EQ(opacities.left, 0.0f);
  EXPECT_EQ(opacities.right, 1.0f);
}

TEST_F(TabGroupStyleSeparatorTest, DraggedGroupHasNoSeparators) {
  const TabStyle::SeparatorOpacities opacities =
      TabGroupStyle::GetSeparatorOpacities(/*is_collapsed=*/true,
                                           /*is_dragging=*/true, kTabCount,
                                           /*adjacent_tab=*/nullptr);
  EXPECT_EQ(opacities.left, 0.0f);
  EXPECT_EQ(opacities.right, 0.0f);
}

TEST_F(TabGroupStyleSeparatorTest, SeparatorsHiddenWhenDecluttered) {
  base::test::ScopedFeatureList feature_list(features::kTabStripDeclutter);

  // Below the threshold the separator is shown.
  EXPECT_EQ(TabGroupStyle::GetSeparatorOpacities(
                /*is_collapsed=*/true, /*is_dragging=*/false,
                TabStyle::kTabStripDeclutterMinTabsForSeparatorHide - 1,
                /*adjacent_tab=*/nullptr)
                .right,
            1.0f);

  // At the threshold separators are hidden.
  EXPECT_EQ(TabGroupStyle::GetSeparatorOpacities(
                /*is_collapsed=*/true, /*is_dragging=*/false,
                TabStyle::kTabStripDeclutterMinTabsForSeparatorHide,
                /*adjacent_tab=*/nullptr)
                .right,
            0.0f);
}

TEST_F(TabGroupStyleSeparatorTest, HiddenNextToTabWithVisibleBackground) {
  FakeTabStyleViewDelegate adjacent_tab;
  auto trailing_opacity = [&adjacent_tab]() {
    return TabGroupStyle::GetSeparatorOpacities(/*is_collapsed=*/true,
                                                /*is_dragging=*/false,
                                                kTabCount, &adjacent_tab)
        .right;
  };

  // An inactive adjacent tab has no background, so the separator is shown.
  EXPECT_EQ(trailing_opacity(), 1.0f);

  adjacent_tab.set_active(true);
  EXPECT_EQ(trailing_opacity(), 0.0f);
  adjacent_tab.set_active(false);

  adjacent_tab.set_selected(true);
  EXPECT_EQ(trailing_opacity(), 0.0f);
  adjacent_tab.set_selected(false);

  adjacent_tab.set_hovering(true);
  EXPECT_EQ(trailing_opacity(), 0.0f);
  adjacent_tab.set_hovering(false);

  EXPECT_EQ(trailing_opacity(), 1.0f);
}

TEST_F(TabGroupStyleSeparatorTest, ShownNextToTabInAnotherGroup) {
  // If the following tab is in another group, that group's header sits between
  // the two, so the adjacent tab's background doesn't matter.
  FakeTabStyleViewDelegate adjacent_tab;
  adjacent_tab.set_group(tab_groups::TabGroupId::GenerateNew());
  adjacent_tab.set_active(true);

  EXPECT_EQ(TabGroupStyle::GetSeparatorOpacities(/*is_collapsed=*/true,
                                                 /*is_dragging=*/false,
                                                 kTabCount, &adjacent_tab)
                .right,
            1.0f);
}

TEST_F(TabGroupStyleSeparatorTest, FadesWithAdjacentTabHoverAnimation) {
  FakeTabStyleViewDelegate adjacent_tab;
  adjacent_tab.set_hover_animation_value(0.25f);

  EXPECT_FLOAT_EQ(TabGroupStyle::GetSeparatorOpacities(
                      /*is_collapsed=*/true, /*is_dragging=*/false, kTabCount,
                      &adjacent_tab)
                      .right,
                  0.75f);
}

TEST_F(TabGroupStyleSeparatorTest, OpacitiesAreInPhysicalOrderInRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");
  ASSERT_TRUE(base::i18n::IsRTL());

  const TabStyle::SeparatorOpacities opacities =
      TabGroupStyle::GetSeparatorOpacities(/*is_collapsed=*/true,
                                           /*is_dragging=*/false, kTabCount,
                                           /*adjacent_tab=*/nullptr);
  EXPECT_EQ(opacities.left, 1.0f);
  EXPECT_EQ(opacities.right, 0.0f);
}

TEST(HorizontalTabStyleViewsSeparatorTest, HasVisibleBackground) {
  FakeTabStyleViewDelegate tab;
  EXPECT_FALSE(HorizontalTabStyleViews::HasVisibleBackground(&tab));

  tab.set_active(true);
  EXPECT_TRUE(HorizontalTabStyleViews::HasVisibleBackground(&tab));
  tab.set_active(false);

  tab.set_selected(true);
  EXPECT_TRUE(HorizontalTabStyleViews::HasVisibleBackground(&tab));
  tab.set_selected(false);

  tab.set_hovering(true);
  EXPECT_TRUE(HorizontalTabStyleViews::HasVisibleBackground(&tab));
  tab.set_hovering(false);

  // Pinned tabs only have a visible background with the new pinned styling.
  tab.set_pinned(true);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({}, {tabs::kNewHorizontalPinnedTabStyling});
    EXPECT_FALSE(HorizontalTabStyleViews::HasVisibleBackground(&tab));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {tabs::kTabStripUnification, tabs::kNewHorizontalPinnedTabStyling}, {});
    EXPECT_TRUE(HorizontalTabStyleViews::HasVisibleBackground(&tab));
  }
}

TEST(HorizontalTabStyleViewsSeparatorTest,
     GetHoverInterpolatedSeparatorOpacity) {
  FakeTabStyleViewDelegate other_tab;
  other_tab.set_hover_animation_value(0.4f);

  // The larger of the two hover values wins.
  EXPECT_FLOAT_EQ(HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
                      /*for_layout=*/false, &other_tab, /*hover_value=*/0.1f),
                  0.6f);
  EXPECT_FLOAT_EQ(HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
                      /*for_layout=*/false, &other_tab, /*hover_value=*/0.7f),
                  0.3f);

  // Layout opacities ignore the other tab's hover state.
  EXPECT_FLOAT_EQ(HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
                      /*for_layout=*/true, &other_tab, /*hover_value=*/0.0f),
                  1.0f);

  // An active other tab's hover state is ignored.
  other_tab.set_active(true);
  EXPECT_FLOAT_EQ(HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
                      /*for_layout=*/false, &other_tab, /*hover_value=*/0.0f),
                  1.0f);

  // No other tab.
  EXPECT_FLOAT_EQ(HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
                      /*for_layout=*/false, /*other_tab=*/nullptr,
                      /*hover_value=*/0.2f),
                  0.8f);
}

TEST(HorizontalTabStyleViewsSeparatorTest,
     GetSeparatorBoundsAreRelativeToOrigin) {
  const gfx::Rect bounds(0, 0, 200, TabStyle::Get()->GetStandardHeight());
  const TabStyle::SeparatorBounds at_origin =
      HorizontalTabStyleViews::GetSeparatorBounds(bounds, /*scale=*/1.0f,
                                                  /*stroke_thickness=*/0);
  const TabStyle::SeparatorBounds offset =
      HorizontalTabStyleViews::GetSeparatorBounds(bounds + gfx::Vector2d(50, 0),
                                                  /*scale=*/1.0f,
                                                  /*stroke_thickness=*/0);

  // Moving the bounds doesn't change the separators' local position.
  EXPECT_EQ(at_origin.leading, offset.leading);
  EXPECT_EQ(at_origin.trailing, offset.trailing);

  // The separators sit inside the bounds, leading before trailing, and have
  // the style's separator size.
  EXPECT_GE(at_origin.leading.x(), 0.0f);
  EXPECT_LE(at_origin.trailing.right(), bounds.width());
  EXPECT_LT(at_origin.leading.x(), at_origin.trailing.x());
  EXPECT_EQ(at_origin.trailing.size(),
            gfx::SizeF(TabStyle::Get()->GetSeparatorSize()));
}
