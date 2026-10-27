// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace translate {

class TranslatePageActionInteractiveUiTest : public InteractiveBrowserTest {
 public:
  TranslatePageActionInteractiveUiTest() = default;

  TranslatePageActionInteractiveUiTest(
      const TranslatePageActionInteractiveUiTest&) = delete;
  TranslatePageActionInteractiveUiTest& operator=(
      const TranslatePageActionInteractiveUiTest&) = delete;

  ~TranslatePageActionInteractiveUiTest() override = default;

  page_actions::PageActionTestAccessor GetTranslateIcon() {
    return page_actions::PageActionTestAccessor(browser(),
                                                kActionShowTranslate);
  }

  views::BubbleDialogDelegate* GetBubble() const {
    return TranslateController::From(browser())->GetTranslateBubble();
  }

  PartialTranslateBubbleView* GetPartialTranslateBubble() {
    return TranslateController::From(browser())->GetPartialTranslateBubble();
  }
};

// Verifies that clicking the Translate icon closes the Partial Translate bubble
// and results in neither of the two Translate bubbles being shown.
IN_PROC_BROWSER_TEST_F(TranslatePageActionInteractiveUiTest,
                       ClosePartialTranslateBubble) {
  TranslateController* controller = TranslateController::From(browser());

  RunTestSequence(
      // Show the Translate icon.
      Do([&]() {
        ChromeTranslateClient::FromWebContents(
            browser()->GetTabStripModel()->GetActiveWebContents())
            ->GetTranslateManager()
            ->GetLanguageState()
            ->SetTranslateEnabled(true);
      }),
      WaitForShow(kTranslatePageActionElementId), Do([&]() {
        controller->StartPartialTranslate("fr", "en", std::u16string());
      }),
      WaitForShow(PartialTranslateBubbleView::kIdentifier),
      // Clicking the icon should close the Partial Translate bubble and should
      // not open the Full Page Translate bubble.
      MoveMouseTo(kTranslatePageActionElementId), ClickMouse(),
      WaitForHide(PartialTranslateBubbleView::kIdentifier),
      CheckResult([&]() { return GetPartialTranslateBubble(); },
                  ::testing::IsNull()),
      CheckResult([&]() { return GetBubble(); }, ::testing::IsNull()));
}

IN_PROC_BROWSER_TEST_F(TranslatePageActionInteractiveUiTest,
                       IconViewAccessibleName) {
  // Show the Translate icon.
  ChromeTranslateClient::FromWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents())
      ->GetTranslateManager()
      ->GetLanguageState()
      ->SetTranslateEnabled(true);

  EXPECT_EQ(
      GetTranslateIcon().GetAccessibleName(),
      chrome::GetCleanText(l10n_util::GetStringUTF16(IDS_SHOW_TRANSLATE)));
  EXPECT_EQ(GetTranslateIcon().GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_TOOLTIP_TRANSLATE));
}

}  // namespace translate
