// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_textfield_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

using chromeos::editor_menu::EditorContext;
using chromeos::editor_menu::EditorMode;
using chromeos::editor_menu::PresetQueryCategory;
using chromeos::editor_menu::PresetTextQuery;
using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Property;
using ::testing::SizeIs;

EditorContext CreateTestEditorPanelContext(EditorMode editor_panel_mode,
                                           bool consent_status_settled) {
  return EditorContext(editor_panel_mode,
                       /*consent_status_settled=*/consent_status_settled,
                       std::vector<PresetTextQuery>{});
}

EditorContext CreateTestEditorPanelContextWithQueries() {
  return EditorContext(
      EditorMode::kRewrite,
      /*consent_status_settled=*/true,
      std::vector<PresetTextQuery>{
          PresetTextQuery("ID1", u"Rephrase", PresetQueryCategory::kRephrase),
          PresetTextQuery("ID2", u"Emojify", PresetQueryCategory::kEmojify),
          PresetTextQuery("ID3", u"Shorten", PresetQueryCategory::kShorten),
          PresetTextQuery("ID4", u"Elaborate", PresetQueryCategory::kElaborate),
          PresetTextQuery("ID5", u"Formalize", PresetQueryCategory::kFormalize),
      });
}

auto ChildrenSizeIs(int n) {
  return Property(&views::View::children, SizeIs(n));
}

constexpr int kMarginDip = 8;
constexpr gfx::Rect kAnchorBounds(500, 300, 80, 160);
constexpr gfx::Rect kAnchorBoundsTop(500, 10, 80, 160);

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

// TODO(crbug.com/41486387): Tentatively disable the failing tests.
#if BUILDFLAG(IS_CHROMEOS)
  void SetUp() override { GTEST_SKIP(); }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class EditorMenuBrowserI18nEnabledTest : public EditorMenuBrowserTest {
 public:
  EditorMenuBrowserI18nEnabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kOrca,
                              chromeos::features::kFeatureManagementOrca,
                              chromeos::features::kOrcaUseL10nStrings},
        /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLang, "fr");
  }

  ~EditorMenuBrowserI18nEnabledTest() override = default;
};

class EditorMenuBrowserI18nDisabledTest : public EditorMenuBrowserTest {
 public:
  EditorMenuBrowserI18nDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            chromeos::features::kOrca,
            chromeos::features::kFeatureManagementOrca,
        },
        /*disabled_features=*/{chromeos::features::kOrcaUseL10nStrings});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLang, "fr");
  }

  ~EditorMenuBrowserI18nDisabledTest() override = default;
};

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureDisabledTest,
                       ShouldNotCreateWhenFeatureNotEnabled) {
  EXPECT_FALSE(chromeos::features::IsOrcaEnabled());
  EXPECT_EQ(nullptr, GetControllerImpl());
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShouldCreateWhenFeatureEnabled) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest, CanShowEditorMenu) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));

  EXPECT_TRUE(views::IsViewClass<EditorMenuView>(GetEditorMenuView()));

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShowsRewriteUIWithChips) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      gfx::Rect(200, 300, 400, 200), CreateTestEditorPanelContextWithQueries());

  // Editor menu should be showing with two rows of chips.
  ASSERT_TRUE(views::IsViewClass<EditorMenuView>(GetEditorMenuView()));
  const auto* chips_container =
      views::AsViewClass<EditorMenuView>(GetEditorMenuView())
          ->chips_container_for_testing();
  EXPECT_THAT(chips_container->children(),
              ElementsAre(::testing::Pointee(ChildrenSizeIs(3)),
                          ::testing::Pointee(ChildrenSizeIs(2))));
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShowsWideRewriteUIWithChips) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  // Show editor menu with a wide anchor.
  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      gfx::Rect(200, 300, 600, 200), CreateTestEditorPanelContextWithQueries());

  // Editor menu should be wide enough to fit all chips in one row.
  ASSERT_TRUE(views::IsViewClass<EditorMenuView>(GetEditorMenuView()));
  const auto* chips_container =
      views::AsViewClass<EditorMenuView>(GetEditorMenuView())
          ->chips_container_for_testing();
  EXPECT_THAT(chips_container->children(),
              ElementsAre(::testing::Pointee(ChildrenSizeIs(5))));
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest, CanShowPromoCard) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kPromoCard,
                                   /*consent_status_settled=*/false));

  EXPECT_TRUE(views::IsViewClass<EditorMenuPromoCardView>(GetEditorMenuView()));

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       DoesNotShowWhenSoftBlocked) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kSoftBlocked,
                                   /*consent_status_settled=*/true));

  EXPECT_EQ(GetControllerImpl()->editor_menu_widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       ShowEditorMenuAboveAnchor) {
  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_NE(nullptr, GetControllerImpl());

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));
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

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBoundsTop,
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));

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

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      anchor_bounds,
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));

  // Editor menu should be right aligned with anchor.
  EXPECT_EQ(GetEditorMenuView()->GetBoundsInScreen().right(),
            anchor_bounds.right());

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       MatchesAnchorWidth) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  // Show editor menu.
  constexpr int kAnchorWidth = 401;
  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      gfx::Rect(200, 300, kAnchorWidth, 50),
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));

  // Editor menu width should match anchor width.
  EXPECT_EQ(GetEditorMenuView()->GetBoundsInScreen().width(), kAnchorWidth);

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       MatchesUpdatedAnchorWidth) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  // Show editor menu.
  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      gfx::Rect(200, 300, 408, 50),
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));
  constexpr int kNewAnchorWidth = 365;
  GetControllerImpl()->OnAnchorBoundsChanged(
      gfx::Rect(200, 300, kNewAnchorWidth, 50));

  // Editor menu width should match the latest anchor width.
  EXPECT_EQ(GetEditorMenuView()->GetBoundsInScreen().width(), kNewAnchorWidth);

  GetEditorMenuView()->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserFeatureEnabledTest,
                       AdjustsPositionWhenAnchorBoundsUpdate) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  // Show editor menu.
  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));
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

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kRewrite,
                                   /*consent_status_settled=*/true));

  ASSERT_NE(GetEditorMenuView()->GetWidget(), nullptr);
  GetEditorMenuView()->GetWidget()->GetFocusManager()->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  EXPECT_TRUE(GetEditorMenuView()->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(
    EditorMenuBrowserI18nEnabledTest,
    ShowWriteCardTitleInFrenchWhenOrcaUseL10nStringsIsEnabled) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds, CreateTestEditorPanelContext(
                         EditorMode::kWrite, /*consent_status_settled=*/true));

  ASSERT_TRUE(views::IsViewClass<EditorMenuView>(GetEditorMenuView()));

  EXPECT_EQ(views::AsViewClass<EditorMenuView>(GetEditorMenuView())
                ->textfield_for_testing()
                ->textfield()
                ->GetPlaceholderText(),
            l10n_util::GetStringUTF16(
                IDS_EDITOR_MENU_FREEFORM_PROMPT_INPUT_FIELD_PLACEHOLDER));
}

IN_PROC_BROWSER_TEST_F(
    EditorMenuBrowserI18nEnabledTest,
    ShowPromoCardTitleInFrenchWhenOrcaUseL10nStringsFlagIsEnabled) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kPromoCard,
                                   /*consent_status_settled=*/false));

  ASSERT_TRUE(views::IsViewClass<EditorMenuPromoCardView>(GetEditorMenuView()));

  EXPECT_EQ(views::AsViewClass<EditorMenuPromoCardView>(GetEditorMenuView())
                ->title_for_testing()
                ->GetDisplayTextForTesting(),
            l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_TITLE));
}

IN_PROC_BROWSER_TEST_F(
    EditorMenuBrowserI18nDisabledTest,
    ShowWriteCardPlaceholderTextInEnUsWhenOrcaUseL10nStringsFlagIsDisabled) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds, CreateTestEditorPanelContext(
                         EditorMode::kWrite, /*consent_status_settled=*/true));

  ASSERT_TRUE(views::IsViewClass<EditorMenuView>(GetEditorMenuView()));

  EXPECT_EQ(views::AsViewClass<EditorMenuView>(GetEditorMenuView())
                ->textfield_for_testing()
                ->textfield()
                ->GetPlaceholderText(),
            u"Enter a prompt");
}

IN_PROC_BROWSER_TEST_F(
    EditorMenuBrowserI18nDisabledTest,
    ShowPromoCardTitleInEnUsWhenOrcaUseL10nStringsFlagIsDisabled) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kPromoCard,
                                   /*consent_status_settled=*/false));

  ASSERT_TRUE(views::IsViewClass<EditorMenuPromoCardView>(GetEditorMenuView()));

  EXPECT_EQ(views::AsViewClass<EditorMenuPromoCardView>(GetEditorMenuView())
                ->title_for_testing()
                ->GetDisplayTextForTesting(),
            u"Write faster and with more confidence");
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserI18nDisabledTest,
                       EditorMenuPromoCardViewAccessibleProperties) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kPromoCard,
                                   /*consent_status_settled=*/false));
  auto* promo_card =
      views::AsViewClass<EditorMenuPromoCardView>(GetEditorMenuView());
  ui::AXNodeData data;

  ASSERT_TRUE(promo_card);
  promo_card->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Write faster and with more confidence");
}

IN_PROC_BROWSER_TEST_F(EditorMenuBrowserI18nEnabledTest,
                       EditorMenuPromoCardViewAccessibleProperties) {
  ASSERT_THAT(GetControllerImpl(), Not(IsNull()));

  GetControllerImpl()->OnGetAnchorBoundsAndEditorContextForTesting(
      kAnchorBounds,
      CreateTestEditorPanelContext(EditorMode::kPromoCard,
                                   /*consent_status_settled=*/false));
  auto* promo_card =
      views::AsViewClass<EditorMenuPromoCardView>(GetEditorMenuView());
  ui::AXNodeData data;

  ASSERT_TRUE(promo_card);
  promo_card->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kDialog, data.role);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_TITLE));
}
