// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

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

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       UploadUnsupportedTextFileUpdatesContextData) {
  auto* omnibox_controller =
      browser()->window()->GetLocationBar()->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->window()->GetNativeWindow());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  file_selector.OpenFileUploadDialog(web_contents,
                                     /*is_image=*/false, &mock_edit_model,
                                     std::nullopt,
                                     /*was_ai_mode_open=*/true);

  // Create a real temporary file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath text_file_path = temp_dir.GetPath().AppendASCII("test.txt");
  ASSERT_TRUE(base::WriteFile(text_file_path, "dummy data"));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_edit_model, OpenAiMode(false, true))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Trigger the file selection.
  file_selector.FileSelected(
      ui::SelectedFileInfo(text_file_path, text_file_path), 0);

  run_loop.Run();

  // Verify that the unsupported "text/plain" file information was successfully
  // injected into the SearchboxContextData for the frontend to handle.
  SearchboxContextData* searchbox_context_data =
      browser()->GetFeatures().searchbox_context_data();
  ASSERT_TRUE(searchbox_context_data);

  auto context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  ASSERT_EQ(context->file_infos.size(), 1u);

  const auto& file_attachment = context->file_infos[0]->get_file_attachment();
  EXPECT_EQ(file_attachment->name, "test.txt");
  EXPECT_EQ(file_attachment->mime_type, "text/plain");
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       RecordHistogramOnFileSelected) {
  base::HistogramTester histogram_tester;

  auto* omnibox_controller =
      browser()->window()->GetLocationBar()->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->window()->GetNativeWindow());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  file_selector.OpenFileUploadDialog(web_contents,
                                     /*is_image=*/false, &mock_edit_model,
                                     std::nullopt,
                                     /*was_ai_mode_open=*/true);

  // Create a real temporary file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath text_file_path = temp_dir.GetPath().AppendASCII("test.txt");
  ASSERT_TRUE(base::WriteFile(text_file_path, "dummy data"));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_edit_model, OpenAiMode(false, true))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Trigger the file selection.
  file_selector.FileSelected(
      ui::SelectedFileInfo(text_file_path, text_file_path), 0);

  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox", 0, 1);
}
