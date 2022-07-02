// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_icon_view.h"

#include <string>
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"

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
};

// Verifies that clicking the Translate icon closes the Partial Translate bubble
// and results in neither of the two Translate bubbles being shown.
IN_PROC_BROWSER_TEST_F(TranslateIconViewTest, ClosePartialTranslateBubble) {
  browser()->window()->ShowPartialTranslateBubble(
      PartialTranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE, "fr",
      "en", std::u16string(), TranslateErrors::NONE);
  EXPECT_THAT(GetPartialTranslateBubble(), ::testing::NotNull());

  // The Translate icon should appear with the Partial Translate bubble
  PageActionIconView* translate_icon = GetTranslateIcon();
  EXPECT_THAT(translate_icon, ::testing::NotNull());

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

}  // namespace translate
