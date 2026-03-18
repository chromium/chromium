// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/window/dialog_client_view.h"

namespace extensions {

using ::testing::Return;
using ::testing::ReturnRef;

class DownloadDangerDialogInteractiveTest : public InteractiveBrowserTest {
 public:
  DownloadDangerDialogInteractiveTest() = default;
  ~DownloadDangerDialogInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Set up basic mock expectations so the dialog can read the danger type
    // and format the warning string correctly.
    ON_CALL(mock_download_item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath(FILE_PATH_LITERAL("evil.exe"))));
    ON_CALL(mock_download_item_, GetDangerType())
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
    ON_CALL(mock_download_item_, IsDangerous()).WillByDefault(Return(true));
  }

 protected:
  // Helper to trigger the dialog and capture the resulting Action.
  auto ShowDialog() {
    return Do([this]() {
      dialog_result_.reset();
      ShowDownloadDangerDialog(
          &mock_download_item_,
          browser()->tab_strip_model()->GetActiveWebContents(),
          base::BindOnce(&DownloadDangerDialogInteractiveTest::OnDialogResolved,
                         base::Unretained(this)));
    });
  }

  // A custom Kombucha check to verify the callback result.
  auto CheckResult(DownloadDangerPrompt::Action expected_action) {
    return Check(
        [this, expected_action]() {
          return dialog_result_.has_value() &&
                 dialog_result_.value() == expected_action;
        },
        "Verify dialog callback result matches expected action.");
  }

  testing::NiceMock<download::MockDownloadItem> mock_download_item_;
  std::optional<DownloadDangerPrompt::Action> dialog_result_;

 private:
  void OnDialogResolved(DownloadDangerPrompt::Action action) {
    dialog_result_ = action;
  }
};

// Tests that clicking the button labeled "Confirm Download" (which is wired
// to the DialogModel's Cancel button) results in the ACCEPT action.
IN_PROC_BROWSER_TEST_F(DownloadDangerDialogInteractiveTest,
                       ClickingKeepAcceptsDanger) {
  RunTestSequence(
      ShowDialog(),
      // Wait for the dialog to appear by waiting for its button.
      WaitForShow(kDownloadDangerDialogKeepButtonElementId),
      // Press the button (which we labeled "Confirm Download").
      PressButton(kDownloadDangerDialogKeepButtonElementId),
      // Ensure the dialog disappears.
      WaitForHide(kDownloadDangerDialogKeepButtonElementId),
      // Verify the extension API backend receives the ACCEPT signal.
      CheckResult(DownloadDangerPrompt::Action::ACCEPT));
}

// Tests that clicking the button labeled "Cancel" (which is wired
// to the DialogModel's OK button) results in the CANCEL action.
IN_PROC_BROWSER_TEST_F(DownloadDangerDialogInteractiveTest,
                       ClickingCancelRejectsDanger) {
  RunTestSequence(
      ShowDialog(),
      // Wait for the dialog to appear by waiting for its button.
      WaitForShow(kDownloadDangerDialogCancelButtonElementId),
      // Press the button (which we labeled "Cancel").
      PressButton(kDownloadDangerDialogCancelButtonElementId),
      // Ensure the dialog disappears.
      WaitForHide(kDownloadDangerDialogCancelButtonElementId),
      // Verify the extension API backend receives the CANCEL signal.
      CheckResult(DownloadDangerPrompt::Action::CANCEL));
}

// Tests that removing the DownloadItem while the dialog is open
// results in the DISMISS action and closes the dialog.
IN_PROC_BROWSER_TEST_F(DownloadDangerDialogInteractiveTest,
                       DownloadDestroyedDismissesDanger) {
  RunTestSequence(
      ShowDialog(), WaitForShow(kDownloadDangerDialogKeepButtonElementId),
      Do([this]() { mock_download_item_.NotifyObserversDownloadRemoved(); }),
      WaitForHide(kDownloadDangerDialogKeepButtonElementId),
      CheckResult(DownloadDangerPrompt::Action::DISMISS));
}

}  // namespace extensions
