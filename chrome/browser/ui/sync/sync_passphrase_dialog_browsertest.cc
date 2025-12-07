// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/sync_passphrase_dialog.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/mock_callback.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
    ui::test::PollingStateObserver<std::u16string_view>,
    kPolledTextfieldContent);
}

class SyncPassphraseDialogBrowserTest : public InteractiveBrowserTest {
 public:
  SyncPassphraseDialogBrowserTest() = default;

  // Sets the password and waits for the state to be propagated.
  // `kPolledTextfieldContent` must be initialized with `PollState()` before
  // this can be used.
  MultiStep SetPassordText(std::u16string_view text) {
    return Steps(EnterText(kTextFieldName, std::u16string(text)),
                 WaitForState(kPolledTextfieldContent, text));
  }

 protected:
  const std::string kTextFieldName = "Texfield";
  const std::string kFooterLabelName = "FooterLabel";
};

IN_PROC_BROWSER_TEST_F(SyncPassphraseDialogBrowserTest, PixelTest) {
  const std::u16string_view kPassphrase = u"Passphrase";

  base::MockRepeatingCallback<bool(std::u16string_view)> decrypt_callback;
  Browser* browser_ptr = browser();
  views::Textfield* textfield = nullptr;

  // Reject the first passphrase, and accept the second.
  EXPECT_CALL(decrypt_callback, Run(kPassphrase))
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  RunTestSequence(
      // Show the dialog.
      Do([&decrypt_callback, browser_ptr] {
        ShowSyncPassphraseDialog(*browser_ptr, decrypt_callback.Get());
      }),
      WaitForShow(kSyncPassphraseOkButtonFieldId),
      // Check that the button is initially disabled.
      WaitForViewProperty(kSyncPassphraseOkButtonFieldId, views::View, Enabled,
                          false),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      ScreenshotSurface(kSyncPassphraseOkButtonFieldId, "InitialState",
                        "5838370"),
      // Find the underlying `views::Textfield` and track its state.
      NameDescendantViewByType<views::Textfield>(kSyncPassphrasePasswordFieldId,
                                                 kTextFieldName),
      WithView(kTextFieldName,
               [&textfield](views::Textfield* t) {
                 CHECK(t);
                 textfield = t;
               }),
      PollState(kPolledTextfieldContent,
                [&textfield]() { return textfield->GetText(); }),
      // Enter some text, the button is enabled.
      SetPassordText(kPassphrase),
      WaitForViewProperty(kSyncPassphraseOkButtonFieldId, views::View, Enabled,
                          true),
      ScreenshotSurface(kSyncPassphraseOkButtonFieldId, "WithText", "5838370"),
      // Submit wrong passphrase, field is cleared and button disabled again.
      PressButton(kSyncPassphraseOkButtonFieldId),
      WaitForState(kPolledTextfieldContent, std::u16string()),
      WaitForViewProperty(kSyncPassphraseOkButtonFieldId, views::View, Enabled,
                          false),
      ScreenshotSurface(kSyncPassphraseOkButtonFieldId, "Invalid", "5838370"),
      // Submit correct passphrase, the dialog closes.
      SetPassordText(kPassphrase), PressButton(kSyncPassphraseOkButtonFieldId),
      WaitForHide(kSyncPassphraseOkButtonFieldId));
}

IN_PROC_BROWSER_TEST_F(SyncPassphraseDialogBrowserTest, FooterLink) {
  Browser* browser_ptr = browser();
  RunTestSequence(
      // Show the dialog.
      Do([browser_ptr] {
        ShowSyncPassphraseDialog(
            *browser_ptr,
            base::BindRepeating([](std::u16string_view) { return false; }));
      }),
      WaitForShow(kSyncPassphraseOkButtonFieldId),
      // Find the footer link.
      NameViewRelative(kSyncPassphraseOkButtonFieldId, kFooterLabelName,
                       [](views::View* ok_button) {
                         return ok_button->GetWidget()
                             ->widget_delegate()
                             ->AsDialogDelegate()
                             ->GetFootnoteViewForTesting();
                       }),
      // Click the link.
      WithView(kFooterLabelName,
               [](views::StyledLabel* footnote) {
                 footnote->ClickFirstLinkForTesting();
               }),
      // Dialog closes.
      WaitForHide(kFooterLabelName));

  // The sync settings page is open and active.
  GURL active_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();
  EXPECT_TRUE(base::StartsWith(active_url.spec(),
                               chrome::kNewSyncGoogleDashboardURL))
      << "active_url: " << active_url;
}

IN_PROC_BROWSER_TEST_F(SyncPassphraseDialogBrowserTest, BrowserCommand) {
  Browser* browser_ptr = browser();
  RunTestSequence(
      // Show the dialog through the browser command.
      Do([browser_ptr] {
        chrome::ExecuteCommand(browser_ptr, IDC_SHOW_SYNC_PASSPHRASE_DIALOG);
      }),
      WaitForShow(kSyncPassphraseOkButtonFieldId));
}
