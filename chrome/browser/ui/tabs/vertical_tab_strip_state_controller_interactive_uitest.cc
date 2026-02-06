// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace base::test {

class VerticalTabStripInteractiveUiTest : public InteractiveBrowserTest {
 public:
  VerticalTabStripInteractiveUiTest() = default;
  ~VerticalTabStripInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    InteractiveBrowserTest::SetUp();
  }

  bool SystemMenuContainsStringId(int message_id) {
    ui::MenuModel* menu_model =
        browser()->GetBrowserView().browser_widget()->GetSystemMenuModel();
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      if (l10n_util::GetStringUTF16(message_id) == menu_model->GetLabelAt(i)) {
        return true;
      }
    }
    return false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/441382208): Unable to programmatically click "show tabs on
// side" in Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTabsToTheSideButton DISABLED_VerifyTabsToTheSideButton
#else
#define MAYBE_VerifyTabsToTheSideButton VerifyTabsToTheSideButton
#endif
// This test checks that we can click the show tabs to the side button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheSideButton) {
  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_VERTICAL_TAB));

  RunTestSequence(
      WaitForShow(kTabStripFrameGrabHandleElementId),
      EnsurePresent(kTabStripFrameGrabHandleElementId),
      MoveMouseTo(kTabStripFrameGrabHandleElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
          SelectMenuItem(SystemMenuModelBuilder::kToggleVerticalTabsElementId)),
      WaitForShow(kVerticalTabStripRegionElementId));

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_HORIZONTAL_TAB));
}

// TODO(crbug.com/441382208): Unable to programmatically click "show tabs on
// top" in Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTabsToTheTopButton DISABLED_VerifyTabsToTheTopButton
#else
#define MAYBE_VerifyTabsToTheTopButton VerifyTabsToTheTopButton
#endif
// This test checks that we can click the show tabs at the top button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheTopButton) {
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_HORIZONTAL_TAB));

  RunScheduledLayouts();

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),
      MoveMouseTo(kVerticalTabStripTopContainerElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
          SelectMenuItem(SystemMenuModelBuilder::kToggleVerticalTabsElementId)),
      WaitForShow(kTabStripFrameGrabHandleElementId));

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_VERTICAL_TAB));
}

struct VerticalTabsBadgeTestParams {
  base::test::FeatureRef testing_feature;
  ui::NewBadgeType expected_badge_type;
};

class VerticalTabStripMenuInteractiveUiTest
    : public ::testing::WithParamInterface<VerticalTabsBadgeTestParams>,
      public InteractiveFeaturePromoTest {
 public:
  VerticalTabStripMenuInteractiveUiTest()
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({GetParam().testing_feature})) {}
  ~VerticalTabStripMenuInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    InteractiveFeaturePromoTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(VerticalTabStripMenuInteractiveUiTest,
                       ShowBadgeInContextMenuToggle) {
  BrowserWidget* const browser_widget =
      BrowserView::GetBrowserViewForBrowser(browser())->browser_widget();
  ui::MenuModel* menu = browser_widget->GetSystemMenuModel();
  size_t command_index = 0u;
  ui::MenuModel::GetModelAndIndexForCommandId(IDC_TOGGLE_VERTICAL_TABS, &menu,
                                              &command_index);
  tabs::VerticalTabStripStateController* const vertical_tabs_controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_FALSE(vertical_tabs_controller->ShouldDisplayVerticalTabs());

  // The badge should show while using the horizontal tab strip.
  std::optional<ui::NewBadgeType> badge_type =
      menu->GetNewBadgeTypeAt(command_index);
  ASSERT_TRUE(badge_type.has_value());
  EXPECT_EQ(badge_type.value(), GetParam().expected_badge_type);

  // While using the vertical tab strip, the badge should be hidden.
  vertical_tabs_controller->SetVerticalTabsEnabled(true);
  menu = browser_widget->GetSystemMenuModel();
  std::optional<ui::NewBadgeType> badge_type_in_vertical_tabs =
      menu->GetNewBadgeTypeAt(command_index);
  EXPECT_FALSE(badge_type_in_vertical_tabs.has_value());

  // Switching back to the horizontal tab strip should show the badge
  // again.
  vertical_tabs_controller->SetVerticalTabsEnabled(false);
  menu = browser_widget->GetSystemMenuModel();
  std::optional<ui::NewBadgeType> badge_type_in_horizontal_tabs =
      menu->GetNewBadgeTypeAt(command_index);
  ASSERT_TRUE(badge_type_in_horizontal_tabs.has_value());
  EXPECT_EQ(badge_type_in_horizontal_tabs.value(),
            GetParam().expected_badge_type);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VerticalTabStripMenuInteractiveUiTest,
    ::testing::Values(
        VerticalTabsBadgeTestParams{
            .testing_feature = tabs::kVerticalTabsPreviewBadge,
            .expected_badge_type = ui::NewBadgeType::kPreview},
        VerticalTabsBadgeTestParams{
            .testing_feature = tabs::kVerticalTabsNewBadge,
            .expected_badge_type = ui::NewBadgeType::kNew}),
    [](const ::testing::TestParamInfo<
        VerticalTabStripMenuInteractiveUiTest::ParamType>& info) {
      return info.param.expected_badge_type == ui::NewBadgeType::kPreview
                 ? "PreviewBadge"
                 : "NewBadge";
    });

}  // namespace base::test
