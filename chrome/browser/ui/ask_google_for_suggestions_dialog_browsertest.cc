// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class AskGoogleForSuggestionsDialogTest : public DialogBrowserTest {
 public:
  AskGoogleForSuggestionsDialogTest() {}

  AskGoogleForSuggestionsDialogTest(const AskGoogleForSuggestionsDialogTest&) =
      delete;
  AskGoogleForSuggestionsDialogTest& operator=(
      const AskGoogleForSuggestionsDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    std::unique_ptr<SpellingBubbleModel> model =
        std::make_unique<SpellingBubbleModel>(
            browser()->profile(),
            browser()->tab_strip_model()->GetActiveWebContents());

    // The toolkit-views version of the dialog does not utilize the anchor_view
    // and origin parameters passed to this function. Pass dummy values.
    chrome::ShowConfirmBubble(browser()->window()->GetNativeWindow(), nullptr,
                              gfx::Point(), std::move(model));
  }
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(AskGoogleForSuggestionsDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
