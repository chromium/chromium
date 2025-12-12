// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"

class OmniboxPopupFileSelectorBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    auto* factory = ui::FakeSelectFileDialog::RegisterFactory();
    factory->SetOpenCallback(base::DoNothing());
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       ReopensAiModeOnCancelIfPreviouslyOpen) {
  auto* omnibox_controller =
      browser()->window()->GetLocationBar()->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->window()->GetNativeWindow());

  file_selector.OpenFileUploadDialog(
      browser()->tab_strip_model()->GetActiveWebContents(),
      /*is_image=*/true, &mock_edit_model, std::nullopt,
      /*was_ai_mode_open=*/true);

  EXPECT_CALL(mock_edit_model, OpenAiMode(false, true));
  file_selector.FileSelectionCanceled();
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       DoesNotReopenAiModeOnCancelIfPreviouslyClosed) {
  auto* omnibox_controller =
      browser()->window()->GetLocationBar()->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->window()->GetNativeWindow());

  file_selector.OpenFileUploadDialog(
      browser()->tab_strip_model()->GetActiveWebContents(),
      /*is_image=*/true, &mock_edit_model, std::nullopt,
      /*was_ai_mode_open=*/false);

  EXPECT_CALL(mock_edit_model, OpenAiMode(testing::_, testing::_)).Times(0);
  file_selector.FileSelectionCanceled();
}
