// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/read_write_cards/read_write_cards_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kMarginDip = 8;
constexpr gfx::Rect kAnchorBounds =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 160));
constexpr gfx::Rect kAnchorBoundsTop =
    gfx::Rect(gfx::Point(500, 0), gfx::Size(80, 160));

}  // namespace

class EditorMenuBrowserTest : public InProcessBrowserTest {
 public:
  EditorMenuBrowserTest() = default;
  ~EditorMenuBrowserTest() override = default;

 protected:
  using EditorMenuControllerImpl =
      chromeos::editor_menu::EditorMenuControllerImpl;
  using EditorMenuPromoCardView =
      chromeos::editor_menu::EditorMenuPromoCardView;

  EditorMenuControllerImpl* GetControllerImpl() {
    return chromeos::ReadWriteCardsFactory::GetForBrowserContext(
               browser()->profile())
        ->editor_menu_for_testing();
  }

  views::View* GetEditorMenuView() {
    return GetControllerImpl()
        ->editor_menu_widget_for_testing()
        ->GetContentsView();
  }

  base::test::ScopedFeatureList feature_list_;
};

class EditorMenuBrowserFeatureEnabledTest : public EditorMenuBrowserTest {
 public:
  EditorMenuBrowserFeatureEnabledTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kOrca);
  }

  ~EditorMenuBrowserFeatureEnabledTest() override = default;
};

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserTest,
                       ShouldNotCreateWhenFeatureNotEnabled) {
  EXPECT_FALSE(chromeos::features::IsOrcaEnabled());
  EXPECT_EQ(nullptr, GetControllerImpl());
};

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShouldCreateWhenFeatureEnabled) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShowEditorMenuAboveAnchor) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());

  GetControllerImpl()->OnTextAvailable(kAnchorBounds, /*selected_text=*/"",
                                       /*surrounding_text=*/"");
  const gfx::Rect& bounds = GetEditorMenuView()->GetBoundsInScreen();

  // View is vertically left aligned with anchor.
  EXPECT_EQ(bounds.x(), kAnchorBounds.x());

  // View is positioned above the anchor.
  EXPECT_EQ(bounds.bottom() + kMarginDip, kAnchorBounds.y());
  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShowEditorMenuBelowAnchor) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());

  GetControllerImpl()->OnTextAvailable(kAnchorBoundsTop, /*selected_text=*/"",
                                       /*surrounding_text=*/"");
  const gfx::Rect& bounds = GetEditorMenuView()->GetBoundsInScreen();

  // View is vertically left aligned with anchor.
  EXPECT_EQ(bounds.x(), kAnchorBoundsTop.x());

  // View is positioned below the anchor.
  EXPECT_EQ(bounds.y() - kMarginDip, kAnchorBoundsTop.bottom());
  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       InitiallyShowsPromoCard) {
  EXPECT_NE(nullptr, GetControllerImpl());

  GetControllerImpl()->OnTextAvailable(kAnchorBounds, /*selected_text=*/"",
                                       /*surrounding_text=*/"");

  EXPECT_TRUE(views::IsViewClass<EditorMenuPromoCardView>(GetEditorMenuView()));

  GetEditorMenuView()->GetWidget()->Close();
}
