// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

using ::testing::IsNull;
using ::testing::Not;

crosapi::mojom::EditorPanelContextPtr CreateTestEditorPanelContext(
    crosapi::mojom::EditorPanelMode editor_panel_mode) {
  auto context = crosapi::mojom::EditorPanelContext::New();
  context->editor_panel_mode = editor_panel_mode;

  return context;
}

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
  using EditorMenuView = chromeos::editor_menu::EditorMenuView;
  using EditorMenuPromoCardView =
      chromeos::editor_menu::EditorMenuPromoCardView;

  EditorMenuControllerImpl* GetControllerImpl() {
    auto* read_write_manager =
        static_cast<chromeos::ReadWriteCardsManagerImpl*>(
            chromeos::ReadWriteCardsManager::Get());
    return read_write_manager->editor_menu_for_testing();
  }

  views::View* GetEditorMenuView() {
    return GetControllerImpl()
        ->editor_menu_widget_for_testing()
        ->GetContentsView();
  }

  base::test::ScopedFeatureList feature_list_;
};

class EditorMenuBrowserFeatureDisabledTest : public EditorMenuBrowserTest {
 public:
  EditorMenuBrowserFeatureDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{chromeos::features::kOrcaDogfood});
  }

  ~EditorMenuBrowserFeatureDisabledTest() override = default;
};

class EditorMenuBrowserFeatureEnabledTest : public EditorMenuBrowserTest {
 public:
  EditorMenuBrowserFeatureEnabledTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kOrca);
  }

  ~EditorMenuBrowserFeatureEnabledTest() override = default;
};

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureDisabledTest,
                       ShouldNotCreateWhenFeatureNotEnabled) {
  EXPECT_FALSE(chromeos::features::IsOrcaEnabled());
  EXPECT_EQ(nullptr, GetControllerImpl());
};

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShouldCreateWhenFeatureEnabled) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest, CanShowEditorMenu) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kRewrite));

  EXPECT_TRUE(views::IsViewClass<EditorMenuView>(GetEditorMenuView()));

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest, CanShowPromoCard) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBounds, CreateTestEditorPanelContext(
                         crosapi::mojom::EditorPanelMode::kPromoCard));

  EXPECT_TRUE(views::IsViewClass<EditorMenuPromoCardView>(GetEditorMenuView()));

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       DoesNotShowWhenBlocked) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kBlocked));

  EXPECT_EQ(GetControllerImpl()->editor_menu_widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShowEditorMenuAboveAnchor) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kRewrite));
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

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBoundsTop,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kRewrite));

  const gfx::Rect& bounds = GetEditorMenuView()->GetBoundsInScreen();

  // View is vertically left aligned with anchor.
  EXPECT_EQ(bounds.x(), kAnchorBoundsTop.x());

  // View is positioned below the anchor.
  EXPECT_EQ(bounds.y() - kMarginDip, kAnchorBoundsTop.bottom());
  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       AlignsEditorMenuRightEdgeWithAnchor) {
  ASSERT_NE(GetControllerImpl(), nullptr);

  // Place the anchor near the right edge of the screen.
  const int screen_right =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().right();
  const gfx::Rect anchor_bounds = gfx::Rect(screen_right - 80, 250, 70, 160);

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      anchor_bounds,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kRewrite));

  // Editor menu should be right aligned with anchor.
  EXPECT_EQ(GetEditorMenuView()->GetBoundsInScreen().right(),
            anchor_bounds.right());

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       AdjustsPositionWhenAnchorBoundsUpdate) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  // Show editor menu.
  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kRewrite));
  const gfx::Rect initial_editor_menu_bounds =
      GetEditorMenuView()->GetBoundsInScreen();
  // Adjust anchor bounds (this can happen e.g. when the context menu adjusts
  // its bounds to not overlap with the shelf, or after a context menu item's
  // icon loads).
  constexpr gfx::Vector2d kAnchorBoundsUpdate(10, -20);
  GetControllerImpl()->OnAnchorBoundsChanged(kAnchorBounds +
                                             kAnchorBoundsUpdate);

  // Editor menu should have been repositioned.
  EXPECT_EQ(GetEditorMenuView()->GetBoundsInScreen(),
            initial_editor_menu_bounds + kAnchorBoundsUpdate);

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       PressingEscClosesEditorMenuWidget) {
  ASSERT_NE(GetControllerImpl(), nullptr);

  GetControllerImpl()->OnGetEditorPanelContextResultForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(crosapi::mojom::EditorPanelMode::kRewrite));

  ASSERT_NE(GetEditorMenuView()->GetWidget(), nullptr);
  GetEditorMenuView()->GetWidget()->GetFocusManager()->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  EXPECT_TRUE(GetEditorMenuView()->GetWidget()->IsClosed());
}
