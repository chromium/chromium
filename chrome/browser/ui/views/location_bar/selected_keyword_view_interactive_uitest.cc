// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"

namespace {

void InputKeys(Browser* browser, const std::vector<ui::KeyboardCode>& keys) {
  for (auto key : keys) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser, key, false, false,
                                                false, false));
  }
}

class SelectedKeywordViewTest : public extensions::ExtensionBrowserTest {
 public:
  SelectedKeywordViewTest() = default;
  ~SelectedKeywordViewTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectedKeywordViewTest);
};

// Tests that an extension's short name is registered as the value of the
// extension's omnibox keyword. When the extension's omnibox keyword is
// activated, then the selected keyword label in the omnibox should be the
// extension's short name.
IN_PROC_BROWSER_TEST_F(SelectedKeywordViewTest,
                       TestSelectedKeywordViewIsExtensionShortname) {
  const extensions::Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("omnibox"), 1);
  ASSERT_TRUE(extension);

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Activate the extension's omnibox keyword.
  InputKeys(browser(), {ui::VKEY_K, ui::VKEY_E, ui::VKEY_Y, ui::VKEY_SPACE});

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SelectedKeywordView* selected_keyword_view =
      browser_view->toolbar()->location_bar()->selected_keyword_view();
  ASSERT_TRUE(selected_keyword_view);

  // Verify that the label in the omnibox is the extension's shortname.
  EXPECT_EQ(extension->short_name(),
            base::UTF16ToUTF8(selected_keyword_view->label()->GetText()));
}

}  // namespace
