// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/views_test_utils.h"

namespace translate {

class TranslateIconViewTest : public InProcessBrowserTest,
                              public ::testing::WithParamInterface<bool> {
 public:
  TranslateIconViewTest() {
    if (IsMigrationEnabled()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kPageActionsMigration,
          {{features::kPageActionsMigrationTranslate.name, "true"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {::features::kPageActionsMigration});
    }
  }

  TranslateIconViewTest(const TranslateIconViewTest&) = delete;
  TranslateIconViewTest& operator=(const TranslateIconViewTest&) = delete;

  ~TranslateIconViewTest() override = default;

  // Returns the page action view that should be enabled for the current
  // feature flag state.
  IconLabelBubbleView* GetTranslateIcon() {
    ToolbarButtonProvider* provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();
    if (IsMigrationEnabled()) {
      return provider->GetPageActionView(kActionShowTranslate);
    }
    return provider->GetPageActionIconView(PageActionIconType::kTranslate);
  }

  views::BubbleDialogDelegate* GetBubble() const {
    return browser()
        ->GetFeatures()
        .translate_bubble_controller()
        ->GetTranslateBubble();
  }

  PartialTranslateBubbleView* GetPartialTranslateBubble() {
    return browser()
        ->GetFeatures()
        .translate_bubble_controller()
        ->GetPartialTranslateBubble();
  }

  std::unique_ptr<views::Widget> CreateTestWidget(
      views::Widget::InitParams::Ownership ownership) {
    auto widget = std::make_unique<views::Widget>();

    views::Widget::InitParams params(ownership,
                                     views::Widget::InitParams::TYPE_WINDOW);
    widget->Init(std::move(params));
    // TODO(https://crbug.com/329235190): The bubble child of a widget that is
    // invisible will not be mapped through wayland and hence never shown so
    // widget must be shown. However, showing widget causes
    // RunAccessibilityPaintChecks() to fail when this feature is disabled due
    // to node_data.GetNameFrom() == kContents.
    if (views::test::IsOzoneBubblesUsingPlatformWidgets()) {
      widget->Show();
    }

    return widget;
  }

  bool IsMigrationEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TranslateIconViewTest,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });

// Verifies that clicking the Translate icon closes the Partial Translate bubble
// and results in neither of the two Translate bubbles being shown.
IN_PROC_BROWSER_TEST_P(TranslateIconViewTest, ClosePartialTranslateBubble) {
  // Show the Translate icon.
  ChromeTranslateClient::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->GetTranslateManager()
      ->GetLanguageState()
      ->SetTranslateEnabled(true);
  auto* translate_icon = GetTranslateIcon();
  EXPECT_THAT(translate_icon, ::testing::NotNull());

  TranslateBubbleController* controller =
      browser()->GetFeatures().translate_bubble_controller();
  auto anchor_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  views::View* anchor_view = anchor_widget->GetContentsView();
  controller->StartPartialTranslate(
      browser()->tab_strip_model()->GetActiveWebContents(), anchor_view,
      nullptr, "fr", "en", std::u16string());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(GetPartialTranslateBubble(), ::testing::NotNull());

  // Clicking the icon should close the Partial Translate bubble and should not
  // open the Full Page Translate bubble.
  base::RunLoop loop;
  ui_test_utils::MoveMouseToCenterAndClick(translate_icon, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           loop.QuitClosure());
  loop.Run();

  EXPECT_THAT(GetPartialTranslateBubble(), ::testing::IsNull());
  EXPECT_THAT(GetBubble(), ::testing::IsNull());
}

IN_PROC_BROWSER_TEST_P(TranslateIconViewTest, IconViewAccessibleName) {
  if (IsMigrationEnabled()) {
    EXPECT_EQ(GetTranslateIcon()->GetViewAccessibility().GetCachedName(),
              BrowserActions::GetCleanTitleAndTooltipText(
                  l10n_util::GetStringUTF16(IDS_SHOW_TRANSLATE)));
  } else {
    EXPECT_EQ(GetTranslateIcon()->GetViewAccessibility().GetCachedName(),
              l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
  }
  EXPECT_EQ(GetTranslateIcon()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
}

}  // namespace translate
