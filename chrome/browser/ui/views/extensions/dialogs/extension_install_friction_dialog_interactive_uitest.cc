// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/styled_label.h"

class ExtensionInstallFrictionDialogUITest : public InteractiveBrowserTest {
 public:
  ExtensionInstallFrictionDialogUITest() = default;
  ~ExtensionInstallFrictionDialogUITest() override = default;
  ExtensionInstallFrictionDialogUITest(
      const ExtensionInstallFrictionDialogUITest&) = delete;
  ExtensionInstallFrictionDialogUITest& operator=(
      const ExtensionInstallFrictionDialogUITest&) = delete;

  auto ShowExtensionInstallFrictionDialog() {
    return Do([&]() {
      extensions::ShowExtensionInstallFrictionDialog(
          browser()->tab_strip_model()->GetActiveWebContents(),
          base::DoNothing());
    });
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallFrictionDialogUITest, ShowDialog) {
  RunTestSequence(
      ShowExtensionInstallFrictionDialog(),
      WaitForShow(extensions::kExtensionInstallFrictionLearnMoreLink));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallFrictionDialogUITest,
                       LearnMoreLinkClosesDialog) {
  RunTestSequence(
      ShowExtensionInstallFrictionDialog(),
      WaitForShow(extensions::kExtensionInstallFrictionLearnMoreLink),

      // Clicking the link closes the dialog.
      WithView(extensions::kExtensionInstallFrictionLearnMoreLink,
               [](views::StyledLabel* learn_more_label) {
                 learn_more_label->ClickFirstLinkForTesting();
               }),
      WaitForHide(extensions::kExtensionInstallFrictionLearnMoreLink));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallFrictionDialogUITest,
                       WebContentsDestroyedClosesDialog) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  const GURL first_url("https://one.com/");
  const GURL second_url("https://two.com/");
  int first_tab_index = 0;

  RunTestSequence(
      // Open two tabs.
      InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, first_url),
      AddInstrumentedTab(kSecondTab, second_url),

      // Activate the first tab.
      SelectTab(kTabStripElementId, first_tab_index),

      // Trigger the dialog.
      ShowExtensionInstallFrictionDialog(),
      // We cannot add an element identifier to the dialog when it's built using
      // DialogModel::Builder. Thus, we check for its existence by checking the
      // visibility of one of its elements.
      WaitForShow(extensions::kExtensionInstallFrictionLearnMoreLink),

      // Close the tab where the dialog is opened.
      Do([&]() {
        browser()->tab_strip_model()->CloseWebContentsAt(
            first_tab_index, TabCloseTypes::CLOSE_NONE);
      }),

      // Dialog should be closed.
      WaitForHide(extensions::kExtensionInstallFrictionLearnMoreLink));
}
