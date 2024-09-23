// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
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
#include "ui/views/test/views_test_utils.h"

namespace translate {

class TranslateIconViewTest : public InProcessBrowserTest {
 public:
  TranslateIconViewTest() = default;

  TranslateIconViewTest(const TranslateIconViewTest&) = delete;
  TranslateIconViewTest& operator=(const TranslateIconViewTest&) = delete;

  ~TranslateIconViewTest() override = default;

  PageActionIconView* GetTranslateIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kTranslate);
  }

  PartialTranslateBubbleView* GetPartialTranslateBubble() {
    return TranslateBubbleController::FromWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
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
};

// Verifies that clicking the Translate icon closes the Partial Translate bubble
// and results in neither of the two Translate bubbles being shown.
IN_PROC_BROWSER_TEST_F(TranslateIconViewTest, ClosePartialTranslateBubble) {
  // Show the Translate icon.
  ChromeTranslateClient::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->GetTranslateManager()
      ->GetLanguageState()
      ->SetTranslateEnabled(true);
  PageActionIconView* translate_icon = GetTranslateIcon();
  EXPECT_THAT(translate_icon, ::testing::NotNull());

  TranslateBubbleController* controller =
      TranslateBubbleController::GetOrCreate(
          browser()->tab_strip_model()->GetActiveWebContents());
  auto anchor_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  views::View* anchor_view = anchor_widget->GetContentsView();
  controller->StartPartialTranslate(anchor_view, nullptr, "fr", "en",
                                    std::u16string());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(GetPartialTranslateBubble(), ::testing::NotNull());

  // Clicking the icon should close the Partial Translate bubble and should not
  // open the Full Page Translate bubble.
  base::RunLoop loop;
  ui_test_utils::MoveMouseToCenterAndPress(translate_icon, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           loop.QuitClosure());
  loop.Run();

  EXPECT_THAT(GetPartialTranslateBubble(), ::testing::IsNull());
  EXPECT_THAT(translate_icon->GetBubble(), ::testing::IsNull());
}

IN_PROC_BROWSER_TEST_F(TranslateIconViewTest, IconViewAccessibleName) {
  EXPECT_EQ(GetTranslateIcon()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
  EXPECT_EQ(GetTranslateIcon()->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
}

}  // namespace translate
