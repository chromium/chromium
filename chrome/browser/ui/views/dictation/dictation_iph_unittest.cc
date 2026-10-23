// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_iph.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace dictation {
namespace {

using user_education::HelpBubbleArrow;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHostElementId);

// A typical visible page area and the bubble size from the UX spec.
constexpr gfx::Rect kPage(0, 0, 1200, 800);
constexpr gfx::Size kBubble(340, 184);

HelpBubbleArrow GetArrow(const gfx::Rect& field,
                         const gfx::Rect& page = kPage,
                         bool is_rtl = false) {
  return GetDictationIphArrowForBounds(field, page, kBubble, is_rtl);
}

TEST(DictationIphArrowTest, PrefersBelow) {
  // Plenty of room on every side.
  EXPECT_EQ(HelpBubbleArrow::kTopLeft, GetArrow({400, 400, 300, 30}));
}

TEST(DictationIphArrowTest, LeftWhenNoRoomBelow) {
  // Field near the bottom of the page with room to its left.
  EXPECT_EQ(HelpBubbleArrow::kRightTop, GetArrow({400, 700, 300, 30}));
}

TEST(DictationIphArrowTest, AccountsForArrowGap) {
  // Room below for the bubble and its arrow, but not for the gap between the
  // arrow and the field, so the bubble goes to the left instead.
  const int field_bottom = kPage.bottom() - kBubble.height() -
                           views::BubbleBorder::kVisibleArrowLength;
  EXPECT_EQ(HelpBubbleArrow::kRightTop,
            GetArrow({400, field_bottom - 30, 300, 30}));
}

TEST(DictationIphArrowTest, RightWhenNoRoomBelowOrLeft) {
  // Field in the bottom-left corner of the page.
  EXPECT_EQ(HelpBubbleArrow::kLeftTop, GetArrow({20, 700, 300, 30}));
}

TEST(DictationIphArrowTest, AboveWhenOnlyRoomAbove) {
  // Narrow page; a wide field near the bottom only has room above it.
  const gfx::Rect page(0, 0, 600, 800);
  EXPECT_EQ(HelpBubbleArrow::kBottomLeft, GetArrow({20, 700, 560, 30}, page));
}

TEST(DictationIphArrowTest, FallbackWhenNothingFits) {
  // A textarea filling most of the page; more space below than above.
  EXPECT_EQ(HelpBubbleArrow::kTopLeft, GetArrow({10, 10, 1180, 700}));
  // More space above than below.
  EXPECT_EQ(HelpBubbleArrow::kBottomLeft, GetArrow({10, 90, 1180, 700}));
}

TEST(DictationIphArrowTest, NarrowFieldSkipsBelowAndAbove) {
  // A field too narrow for a top or bottom arrow uses a side arrow instead,
  // even though there is room below.
  EXPECT_EQ(HelpBubbleArrow::kRightTop, GetArrow({400, 400, 10, 30}));
}

TEST(DictationIphArrowTest, FieldPartiallyOffscreenUsesVisibleWidth) {
  // Only 10px of the field is visible, so it counts as a narrow field. There
  // is no room on its left, so the bubble goes to the right.
  EXPECT_EQ(HelpBubbleArrow::kLeftTop, GetArrow({-290, 400, 300, 30}));
}

TEST(DictationIphArrowTest, RtlBelowAndAboveAreUnchanged) {
  // The bubble dialog mirrors these itself, keeping the bubble start-aligned.
  EXPECT_EQ(HelpBubbleArrow::kTopLeft,
            GetArrow({400, 400, 300, 30}, kPage, /*is_rtl=*/true));
  const gfx::Rect narrow_page(0, 0, 600, 800);
  EXPECT_EQ(HelpBubbleArrow::kBottomLeft,
            GetArrow({20, 700, 560, 30}, narrow_page, /*is_rtl=*/true));
}

TEST(DictationIphArrowTest, RtlSideArrowsArePreMirrored) {
  // Bubble to the left of the field.
  EXPECT_EQ(HelpBubbleArrow::kLeftTop,
            GetArrow({400, 700, 300, 30}, kPage, /*is_rtl=*/true));
  // Bubble to the right of the field.
  EXPECT_EQ(HelpBubbleArrow::kRightTop,
            GetArrow({20, 700, 300, 30}, kPage, /*is_rtl=*/true));
}

// Tests `GetDictationIphArrow()`, which reads the bounds from a real anchor.
class DictationIphArrowForAnchorTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetBounds(gfx::Rect(0, 0, 800, 600));
    // Stands in for the `ContentsWebView` that hosts the anchor. Like it, the
    // host needs an element identifier.
    host_ = widget_->SetContentsView(std::make_unique<views::View>());
    host_->SetProperty(views::kElementIdentifierKey, kHostElementId);
    widget_->Show();
    widget_->LayoutRootViewIfNecessary();
  }

  void TearDown() override {
    host_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> host_ = nullptr;
};

TEST_F(DictationIphArrowForAnchorTest, UsesHostBoundsAsVisibleArea) {
  views::ViewSubregionAnchor anchor(kDictationIphAnchorElementId, *host_);
  // A field near the bottom of the host, with room on its left.
  anchor.MaybeUpdateAnchor(gfx::Rect(400, 500, 300, 30));
  EXPECT_EQ(HelpBubbleArrow::kRightTop, GetDictationIphArrow(&anchor));
}

TEST(DictationIphArrowTest, OtherAnchorTypesUseDefaultArrow) {
  const ui::test::TestElement element(
      kDictationIphAnchorElementId,
      ui::ElementContext::CreateFakeContextForTesting(1));
  EXPECT_EQ(HelpBubbleArrow::kTopLeft, GetDictationIphArrow(&element));
}

}  // namespace
}  // namespace dictation
