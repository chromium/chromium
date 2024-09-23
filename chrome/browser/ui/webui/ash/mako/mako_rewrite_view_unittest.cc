// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_rewrite_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

class TestWebUIContentsWrapper final : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(GURL(""),
                             profile,
                             /*task_manager_string_id=*/0,
                             /*webui_resizes_host=*/true,
                             /*esc_closes_ui=*/false,
                             /*supports_draggable_regions=*/false,
                             /*webui_name=*/"Test") {}
  TestWebUIContentsWrapper(const TestWebUIContentsWrapper&) = delete;
  TestWebUIContentsWrapper& operator=(const TestWebUIContentsWrapper&) = delete;
  ~TestWebUIContentsWrapper() override = default;

  // WebUIContentsWrapper:
  void ReloadWebContents() override {}
  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};

class MakoRewriteViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kOrcaResizingSupport);
    ChromeViewsTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MakoRewriteViewTest, ResizesToWebViewSize) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, /*caret_bounds=*/gfx::Rect(20, 20),
      /*can_fallback_to_center_position=*/false);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  constexpr gfx::Size kWebViewSize(440, 343);
  mako_rewrite_view_ptr->ShowUI();

  EXPECT_EQ(mako_rewrite_view_ptr->GetBoundsInScreen().size(), kWebViewSize);
}

TEST_F(MakoRewriteViewTest, DefaultBoundsAtBottomLeftOfCaret) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  constexpr gfx::Rect kCaretBounds(30, 40, 0, 10);
  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, kCaretBounds,
      /*can_fallback_to_center_position=*/false);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  mako_rewrite_view_ptr->ShowUI();

  // Should be left aligned and below the caret.
  EXPECT_EQ(mako_rewrite_view_ptr->GetBoundsInScreen().x(), kCaretBounds.x());
  EXPECT_GE(mako_rewrite_view_ptr->GetBoundsInScreen().y(),
            kCaretBounds.bottom());
}

TEST_F(MakoRewriteViewTest, AtTopLeftOfCaretForCaretAtScreenBottom) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  const int screen_bottom =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().bottom();
  const gfx::Rect caret_bounds(30, screen_bottom - 20, 0, 10);
  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, caret_bounds,
      /*can_fallback_to_center_position=*/false);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  mako_rewrite_view_ptr->ShowUI();

  // Should be left aligned and above the caret.
  EXPECT_EQ(mako_rewrite_view_ptr->GetBoundsInScreen().x(), caret_bounds.x());
  EXPECT_LE(mako_rewrite_view_ptr->GetBoundsInScreen().bottom(),
            caret_bounds.y());
}

TEST_F(MakoRewriteViewTest, OnScreenWithoutOverlapForSmallSelection) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  constexpr gfx::Rect kSelectionBounds(100, 40, 200, 100);
  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, kSelectionBounds,
      /*can_fallback_to_center_position=*/false);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  mako_rewrite_view_ptr->ShowUI();

  EXPECT_TRUE(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().Contains(
          mako_rewrite_view_ptr->GetBoundsInScreen()));
  EXPECT_FALSE(
      mako_rewrite_view_ptr->GetBoundsInScreen().Intersects(kSelectionBounds));
}

TEST_F(MakoRewriteViewTest, OnScreenForLargeSelection) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  const gfx::Rect selection_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, selection_bounds,
      /*can_fallback_to_center_position=*/false);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  mako_rewrite_view_ptr->ShowUI();

  EXPECT_TRUE(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().Contains(
          mako_rewrite_view_ptr->GetBoundsInScreen()));
}

TEST_F(MakoRewriteViewTest, FallbackToScreenCenterIfCaretBoundsAreEmpty) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  // Simulate a situation when the client fails to report selection bounds.
  constexpr gfx::Size kWebViewSize(440, 343);
  constexpr gfx::Rect kSelectionBounds(0, 0, 0, 0);
  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, kSelectionBounds,
      /*can_fallback_to_center_position=*/true);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  mako_rewrite_view_ptr->ShowUI();

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  EXPECT_EQ(mako_rewrite_view_ptr->GetBoundsInScreen().x(),
            work_area.x() + work_area.width() / 2 - kWebViewSize.width() / 2);
}

TEST_F(MakoRewriteViewTest, FallbackToScreenCenterIfCaretBoundsAreInvalid) {
  TestingProfile profile;
  TestWebUIContentsWrapper contents_wrapper(&profile);

  // Simulate a situation when the client reports invalid bounds
  constexpr gfx::Size kWebViewSize(440, 343);
  constexpr gfx::Rect kSelectionBounds(0, -10000, 100, 100);
  auto mako_rewrite_view = std::make_unique<MakoRewriteView>(
      &contents_wrapper, kSelectionBounds,
      /*can_fallback_to_center_position=*/true);
  auto* mako_rewrite_view_ptr = mako_rewrite_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(mako_rewrite_view));

  mako_rewrite_view_ptr->ShowUI();

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_EQ(mako_rewrite_view_ptr->GetBoundsInScreen().x(),
            work_area.x() + work_area.width() / 2 - kWebViewSize.width() / 2);
}

}  // namespace
}  // namespace ash
