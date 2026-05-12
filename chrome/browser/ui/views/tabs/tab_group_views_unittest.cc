// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_views.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

class TabGroupViewsTest : public ChromeViewsTestBase {
 public:
  TabGroupViewsTest() = default;
  TabGroupViewsTest(const TabGroupViewsTest&) = delete;
  TabGroupViewsTest& operator=(const TabGroupViewsTest&) = delete;
  ~TabGroupViewsTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    tab_container_ = widget_->SetContentsView(std::make_unique<views::View>());
    tab_container_->SetBounds(0, 0, 1000, 100);
    drag_context_ =
        tab_container_->AddChildView(std::make_unique<views::View>());
    drag_context_->SetBounds(0, 0, 1000, 100);

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_slot_controller_ =
        std::make_unique<FakeTabSlotController>(tab_strip_controller_.get());
    group_views_ = std::make_unique<TabGroupViews>(
        tab_container_.get(), drag_context_.get(),
        *(tab_slot_controller_.get()), id_);
  }

  void TearDown() override {
    drag_context_ = nullptr;
    tab_container_ = nullptr;

    widget_->Close();

    group_views_.reset();
    widget_.reset();
    tab_slot_controller_.reset();
    tab_strip_controller_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  void SetGroupTitle(std::u16string title) {
    tab_strip_controller_->SetVisualDataForGroup(
        id_, tab_groups::TabGroupVisualData(
                 std::move(title), tab_groups::TabGroupColorId::kGrey, false));
    group_views_->OnGroupVisualsChanged();
  }

  views::View* title_chip() { return group_views_->header()->children()[0]; }

  views::View* title_label() { return title_chip()->children()[0]; }

  int CenteredTitleX() {
    return (title_chip()->width() - title_label()->width()) / 2;
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> tab_container_;
  raw_ptr<views::View> drag_context_;
  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabSlotController> tab_slot_controller_;
  tab_groups::TabGroupId id_ = tab_groups::TabGroupId::GenerateNew();
  std::unique_ptr<TabGroupViews> group_views_;
};

TEST_F(TabGroupViewsTest, GroupViewsCreated) {
  EXPECT_NE(nullptr, group_views_->header());
  EXPECT_NE(nullptr, group_views_->underline());
  EXPECT_NE(nullptr, group_views_->drag_underline());
  EXPECT_NE(nullptr, group_views_->highlight());

  EXPECT_EQ(tab_container_.get(), group_views_->header()->parent());
  EXPECT_EQ(tab_container_.get(), group_views_->underline()->parent());
  EXPECT_EQ(drag_context_.get(), group_views_->drag_underline()->parent());
  EXPECT_EQ(drag_context_.get(), group_views_->highlight()->parent());
}

TEST_F(TabGroupViewsTest, HeaderInitialAccessibilityProperties) {
  TabGroupHeader* header = group_views_->header();
  ui::AXNodeData node_data;

  header->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kEditable));
  EXPECT_EQ(node_data.role, ax::mojom::Role::kTabList);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
}

TEST_F(TabGroupViewsTest, HeaderTitleIsCentered) {
  SetGroupTitle(u"a");

  EXPECT_EQ(CenteredTitleX(), title_label()->x());
}

// The visual centering offset for color-emoji titles only applies on macOS
// (Apple Color Emoji has asymmetric side bearings). The tests below verify
// that offset is applied. They are macOS-only because shaping color emoji
// requires a color-emoji font with PNG-encoded glyphs, which is not available
// in the unit_tests environment on Linux/Windows (the Skia PNG decoder is not
// registered there, causing GetStringSize to crash).
#if BUILDFLAG(IS_MAC)
TEST_F(TabGroupViewsTest, SingleEmojiHeaderTitleIsVisuallyCentered) {
  // Emoji-presentation codepoint (U+1F60A SMILING FACE WITH SMILING EYES).
  SetGroupTitle(u"\U0001F60A");
  EXPECT_EQ(CenteredTitleX() + 1, title_label()->x());
}

TEST_F(TabGroupViewsTest, SingleEmojiHeaderTitleIsVisuallyCenteredInRtl) {
  base::i18n::SetRTLForTesting(true);

  SetGroupTitle(u"\U0001F60A");
  EXPECT_EQ(CenteredTitleX() - 1, title_label()->x());

  base::i18n::SetRTLForTesting(false);
}

// Skin-tone modifier sequence: thumbs-up + medium skin tone. Two codepoints,
// one grapheme.
TEST_F(TabGroupViewsTest, SkinToneEmojiHeaderTitleIsVisuallyCentered) {
  SetGroupTitle(u"\U0001F44D\U0001F3FD");
  EXPECT_EQ(CenteredTitleX() + 1, title_label()->x());
}

// ZWJ sequence: family (man, woman, girl, boy joined with U+200D). Multiple
// codepoints, one grapheme.
TEST_F(TabGroupViewsTest, ZwjEmojiHeaderTitleIsVisuallyCentered) {
  SetGroupTitle(u"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  EXPECT_EQ(CenteredTitleX() + 1, title_label()->x());
}

// Regional indicator pair forming the U.S. flag. Two codepoints, one
// grapheme.
TEST_F(TabGroupViewsTest, FlagEmojiHeaderTitleIsVisuallyCentered) {
  SetGroupTitle(u"\U0001F1FA\U0001F1F8");
  EXPECT_EQ(CenteredTitleX() + 1, title_label()->x());
}

// Text-default codepoint with Variation Selector-16 (U+263A + U+FE0F).
// Use conservative behavior here and do not apply the emoji offset.
TEST_F(TabGroupViewsTest, EmojiVariationSelectorTitleIsCentered) {
  SetGroupTitle(u"\u263A\uFE0F");
  EXPECT_EQ(CenteredTitleX(), title_label()->x());
}

// Regression: some text-default symbols followed by VS-16 (e.g. diamond suit)
// can still render with text-like metrics in this UI context, so applying the
// emoji offset makes them appear visually right-biased.
TEST_F(TabGroupViewsTest, TextLikeVs16SymbolTitleIsCentered) {
  SetGroupTitle(u"\u2666\uFE0F");
  EXPECT_EQ(CenteredTitleX(), title_label()->x());
}

// Multiple emoji should not receive the single-grapheme visual offset.
TEST_F(TabGroupViewsTest, MultiEmojiHeaderTitleIsCentered) {
  SetGroupTitle(u"\U0001F60A\U0001F60A");
  EXPECT_EQ(CenteredTitleX(), title_label()->x());
}
#endif  // BUILDFLAG(IS_MAC)

// Underline should actually underline the group.
TEST_F(TabGroupViewsTest, UnderlineBoundsNoDrag) {
  TabGroupHeader* header = group_views_->header();
  Tab* tab_1 = tab_container_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(1), tab_slot_controller_.get()));
  tab_1->SetGroup(id_);
  Tab* tab_2 = tab_container_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(2), tab_slot_controller_.get()));
  tab_2->SetGroup(id_);

  header->SetBounds(0, 0, 100, 0);
  tab_1->SetBounds(50, 0, 100, 0);
  tab_2->SetBounds(100, 0, 100, 0);

  group_views_->UpdateBounds();

  EXPECT_TRUE(group_views_->underline()->GetVisible());
  const gfx::Rect underline_bounds = group_views_->underline()->bounds();

  // Underline should begin within the header.
  EXPECT_GT(underline_bounds.x(), header->bounds().x());
  EXPECT_LT(underline_bounds.x(), header->bounds().right());

  // Underline should end within the last tab.
  EXPECT_GT(underline_bounds.right(), tab_2->bounds().x());
  EXPECT_LT(underline_bounds.right(), tab_2->bounds().right());

  EXPECT_FALSE(group_views_->drag_underline()->GetVisible());
}

// Underline should not be visible with chrome refresh flag when only header is
// visible.
TEST_F(TabGroupViewsTest, UnderlineBoundsWhenTabsAreNotVisible) {
  TabGroupHeader* header = group_views_->header();
  Tab* tab_1 = tab_container_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(1), tab_slot_controller_.get()));
  tab_1->SetGroup(id_);
  Tab* tab_2 = tab_container_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(2), tab_slot_controller_.get()));
  tab_2->SetGroup(id_);

  header->SetBounds(0, 0, 100, 0);
  tab_1->SetBounds(50, 0, 100, 0);
  tab_2->SetBounds(100, 0, 100, 0);

  tab_1->SetVisible(false);
  tab_2->SetVisible(false);
  group_views_->UpdateBounds();

  EXPECT_FALSE(group_views_->underline()->GetVisible());
  EXPECT_GT(group_views_->underline()->width(), 0);
}

// Drag_underline should underline the group when the group is being dragged,
// and the highlight should highlight it.
TEST_F(TabGroupViewsTest, UnderlineBoundsHeaderDrag) {
  TabGroupHeader* header = group_views_->header();
  drag_context_->AddChildViewRaw(header);
  Tab* tab_1 = drag_context_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(1), tab_slot_controller_.get()));
  tab_1->SetGroup(id_);
  Tab* tab_2 = drag_context_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(2), tab_slot_controller_.get()));
  tab_2->SetGroup(id_);

  header->SetBounds(0, 0, 100, 0);
  tab_1->SetBounds(50, 0, 100, 0);
  tab_2->SetBounds(100, 0, 100, 0);
  group_views_->highlight()->SetVisible(true);

  group_views_->UpdateBounds();

  // The underline and the drag underline should match exactly.
  EXPECT_TRUE(group_views_->underline()->GetVisible());
  EXPECT_EQ(group_views_->underline()->bounds(),
            group_views_->drag_underline()->bounds());

  EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
  const gfx::Rect drag_underline_bounds =
      group_views_->drag_underline()->bounds();

  // Drag underline should begin within the header.
  EXPECT_GT(drag_underline_bounds.x(), header->bounds().x());
  EXPECT_LT(drag_underline_bounds.x(), header->bounds().right());

  // Drag underline should end within the last tab.
  EXPECT_GT(drag_underline_bounds.right(), tab_2->bounds().x());
  EXPECT_LT(drag_underline_bounds.right(), tab_2->bounds().right());

  // Highlight should span the dragged views exactly.
  EXPECT_EQ(group_views_->highlight()->bounds().x(), header->bounds().x());
  EXPECT_EQ(group_views_->highlight()->bounds().right(),
            tab_2->bounds().right());
}

// Underline and drag_underline should align with one another correctly when
// dragging a tab within a group.
TEST_F(TabGroupViewsTest, UnderlineBoundsDragTabInGroup) {
  TabGroupHeader* header = group_views_->header();
  Tab* other_tab = tab_container_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(1), tab_slot_controller_.get()));
  other_tab->SetGroup(id_);
  Tab* dragged_tab = drag_context_->AddChildView(
      std::make_unique<Tab>(tabs::TabHandle(2), tab_slot_controller_.get()));
  dragged_tab->SetGroup(id_);

  header->SetBounds(0, 0, 100, 0);
  other_tab->SetBounds(50, 0, 100, 0);
  dragged_tab->SetBounds(100, 0, 100, 0);
  group_views_->highlight()->SetVisible(true);

  /////////////// Case 1: `dragged_tab` is right of `other_tab`. ///////////////
  {
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within the header.
    EXPECT_GT(underline_bounds.x(), header->bounds().x());
    EXPECT_LT(underline_bounds.x(), header->bounds().right());

    // Underline should end within the last tab.
    EXPECT_GT(underline_bounds.right(), dragged_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), dragged_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline should begin right at the beginning of the dragged tab.
    EXPECT_EQ(drag_underline_bounds.x(), dragged_tab->x());

    // Drag underline end should match the other underline's.
    EXPECT_EQ(drag_underline_bounds.right(), underline_bounds.right());
  }

  //// Case 2: `dragged_tab` is a bit left of and overlapping `other_tab`. /////
  {
    dragged_tab->SetBounds(45, 0, 100, 0);
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within the header.
    EXPECT_GT(underline_bounds.x(), header->bounds().x());
    EXPECT_LT(underline_bounds.x(), header->bounds().right());

    // Underline should end within the last tab, now `other_tab`.
    EXPECT_GT(underline_bounds.right(), other_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), other_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline should begin right at the beginning of the dragged tab.
    EXPECT_EQ(drag_underline_bounds.x(), dragged_tab->x());

    // Drag underline end should match the other underline's. Note that this is
    // different from the case above because the drag underline must be extended
    // from its natural end to meet the other underline.
    EXPECT_EQ(drag_underline_bounds.right(), underline_bounds.right());
  }

  ///// Case 3: `dragged_tab` is a bit right of and overlapping `header`. //////
  {
    dragged_tab->SetBounds(5, 0, 100, 0);
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within the header.
    EXPECT_GT(underline_bounds.x(), header->bounds().x());
    EXPECT_LT(underline_bounds.x(), header->bounds().right());

    // Underline should end within the last tab, now `other_tab`.
    EXPECT_GT(underline_bounds.right(), other_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), other_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline begin should match the other underline's. Unlike case 4,
    // the drag underline must be extended to match the underline.
    EXPECT_EQ(drag_underline_bounds.x(), underline_bounds.x());

    // Drag underline end should match the dragged tab's end.
    EXPECT_EQ(drag_underline_bounds.right(), dragged_tab->bounds().right());
  }

  ///////////////// Case 4: `dragged_tab` is left of `header`. /////////////////
  {
    dragged_tab->SetBounds(-50, 0, 100, 0);
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within `dragged_tab`, now that it's leftmost.
    EXPECT_GT(underline_bounds.x(), dragged_tab->bounds().x());
    EXPECT_LT(underline_bounds.x(), dragged_tab->bounds().right());

    // Underline should end within the last tab, now `other_tab`.
    EXPECT_GT(underline_bounds.right(), other_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), other_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline begin should match the other underline's. Unlike case 3,
    // this is the drag underline's natural start point.
    EXPECT_EQ(drag_underline_bounds.x(), underline_bounds.x());

    // Drag underline end should match the dragged tab's end.
    EXPECT_EQ(drag_underline_bounds.right(), dragged_tab->bounds().right());
  }
}
