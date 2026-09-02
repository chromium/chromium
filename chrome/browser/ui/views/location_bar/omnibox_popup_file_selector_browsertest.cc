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
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

class OmniboxPopupFileSelectorBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxPopupFileSelectorBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        lens::features::kLensSendRawFileMediaTypes);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    auto* factory = ui::FakeSelectFileDialog::RegisterFactory();
    factory->SetOpenCallback(base::DoNothing());
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       ReopensAiModeOnCancelIfPreviouslyOpen) {
  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  file_selector.OpenFileUploadDialog(
      browser()->GetTabStripModel()->GetActiveWebContents(),
      /*is_image=*/true, &mock_edit_model, std::nullopt,
      /*was_ai_mode_open=*/true);

  EXPECT_CALL(mock_edit_model,
              OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu));
  file_selector.FileSelectionCanceled();
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       DoesNotReopenAiModeOnCancelIfPreviouslyClosed) {
  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  file_selector.OpenFileUploadDialog(
      browser()->GetTabStripModel()->GetActiveWebContents(),
      /*is_image=*/true, &mock_edit_model, std::nullopt,
      /*was_ai_mode_open=*/false);

  EXPECT_CALL(mock_edit_model, OpenAiMode(testing::_)).Times(0);
  file_selector.FileSelectionCanceled();
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       UploadUnsupportedTextFileUpdatesContextData) {
  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
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
  EXPECT_CALL(mock_edit_model,
              OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Trigger the file selection.
  file_selector.FileSelected(
      ui::SelectedFileInfo(text_file_path, text_file_path), 0);

  run_loop.Run();

  // Verify that the unsupported "text/plain" file information was successfully
  // injected into the SearchboxContextData for the frontend to handle.
  SearchboxContextData* searchbox_context_data =
      SearchboxContextData::From(browser());
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

  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
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
  EXPECT_CALL(mock_edit_model,
              OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Trigger the file selection.
  file_selector.FileSelected(
      ui::SelectedFileInfo(text_file_path, text_file_path), 0);

  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox", 0, 1);
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       UploadLimitExceededTriggersMaxImagesError) {
  // Arrange.
  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();

  file_selector.OpenFileUploadDialog(web_contents,
                                     /*is_image=*/true, &mock_edit_model,
                                     std::nullopt,
                                     /*was_ai_mode_open=*/false);

  std::vector<ui::SelectedFileInfo> files;
  for (int i = 0; i < 11; ++i) {
    base::FilePath path(FILE_PATH_LITERAL("image.png"));
    files.emplace_back(path, path);
  }

  EXPECT_CALL(mock_edit_model, OpenAiMode(testing::_)).Times(0);

  // Act.
  file_selector.MultiFilesSelected(files);

  // Assert.
  SearchboxContextData* searchbox_context_data =
      SearchboxContextData::From(browser());
  ASSERT_TRUE(searchbox_context_data);

  auto context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  ASSERT_EQ(context->file_infos.size(), 1u);

  const auto& file_attachment = context->file_infos[0]->get_file_attachment();
  EXPECT_EQ(file_attachment->error_type.value(),
            contextual_search::ContextUploadErrorType::
                kBrowserProcessingMaxImagesExceededError);
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       UploadLimitExceededTriggersMaxPdfsError) {
  // Arrange.
  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();

  file_selector.OpenFileUploadDialog(web_contents,
                                     /*is_image=*/false, &mock_edit_model,
                                     std::nullopt,
                                     /*was_ai_mode_open=*/false);

  std::vector<ui::SelectedFileInfo> files;
  for (int i = 0; i < 11; ++i) {
    base::FilePath path(FILE_PATH_LITERAL("document.pdf"));
    files.emplace_back(path, path);
  }

  EXPECT_CALL(mock_edit_model, OpenAiMode(testing::_)).Times(0);

  // Act.
  file_selector.MultiFilesSelected(files);

  // Assert.
  SearchboxContextData* searchbox_context_data =
      SearchboxContextData::From(browser());
  ASSERT_TRUE(searchbox_context_data);

  auto context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  ASSERT_EQ(context->file_infos.size(), 1u);

  const auto& file_attachment = context->file_infos[0]->get_file_attachment();
  EXPECT_EQ(file_attachment->error_type.value(),
            contextual_search::ContextUploadErrorType::
                kBrowserProcessingMaxPdfsExceededError);
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorBrowserTest,
                       UploadLimitExceededTriggersMaxFilesError) {
  // Arrange.
  auto* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                 ->GetLocationBar()
                                 ->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();

  file_selector.OpenFileUploadDialog(web_contents,
                                     /*is_image=*/false, &mock_edit_model,
                                     std::nullopt,
                                     /*was_ai_mode_open=*/false);

  std::vector<ui::SelectedFileInfo> files;
  for (int i = 0; i < 11; ++i) {
    base::FilePath path(FILE_PATH_LITERAL("file.txt"));
    files.emplace_back(path, path);
  }

  EXPECT_CALL(mock_edit_model, OpenAiMode(testing::_)).Times(0);

  // Act.
  file_selector.MultiFilesSelected(files);

  // Assert.
  SearchboxContextData* searchbox_context_data =
      SearchboxContextData::From(browser());
  ASSERT_TRUE(searchbox_context_data);

  auto context = searchbox_context_data->TakePendingContext();
  ASSERT_TRUE(context);
  ASSERT_EQ(context->file_infos.size(), 1u);

  const auto& file_attachment = context->file_infos[0]->get_file_attachment();
  EXPECT_EQ(file_attachment->error_type.value(),
            contextual_search::ContextUploadErrorType::
                kBrowserProcessingMaxFilesExceededError);
}

class OmniboxPopupFileSelectorAimBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxPopupFileSelectorAimBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {omnibox::internal::kWebUIOmniboxAimPopup,
         omnibox::internal::kWebUIOmniboxPopup,
         omnibox::kOmniboxKeepOpenOnFileSelection},
        {lens::features::kLensSendRawFileMediaTypes,
         features::kWebUILocationBar});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    auto* factory = ui::FakeSelectFileDialog::RegisterFactory();
    factory->SetOpenCallback(base::DoNothing());
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupFileSelectorAimBrowserTest,
                       CreatesAndReleasesDeactivationBlockerWhenAimPopupOpen) {
  auto* location_bar_view = BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar()
                                ->location_bar_view();
  location_bar_view->GetOmniboxController()
      ->popup_state_manager()
      ->SetPopupState(OmniboxPopupState::kAim);

  auto* presenter = location_bar_view->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);
  presenter->Show();

  auto* omnibox_controller = location_bar_view->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  EXPECT_FALSE(presenter->has_active_blockers());

  file_selector.OpenFileUploadDialog(
      browser()->GetTabStripModel()->GetActiveWebContents(),
      /*is_image=*/true, &mock_edit_model, std::nullopt,
      /*was_ai_mode_open=*/true);

  EXPECT_TRUE(presenter->has_active_blockers());

  EXPECT_CALL(mock_edit_model,
              OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu));
  file_selector.FileSelectionCanceled();

  EXPECT_FALSE(presenter->has_active_blockers());
}

class OmniboxPopupFileSelectorClassicWebuiBrowserTest
    : public InProcessBrowserTest {
 public:
  OmniboxPopupFileSelectorClassicWebuiBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::kOmniboxKeepOpenOnFileSelection},
        {omnibox::internal::kWebUIOmniboxAimPopup,
         lens::features::kLensSendRawFileMediaTypes,
         features::kWebUILocationBar});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    auto* factory = ui::FakeSelectFileDialog::RegisterFactory();
    factory->SetOpenCallback(base::DoNothing());
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OmniboxPopupFileSelectorClassicWebuiBrowserTest,
    CreatesAndReleasesDeactivationBlockerWhenClassicPopupOpen) {
  auto* location_bar_view = BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar()
                                ->location_bar_view();
  auto* popup_view = location_bar_view->GetOmniboxPopupView();
  ASSERT_TRUE(popup_view);
  auto* presenter = popup_view->presenter();
  ASSERT_TRUE(presenter);

  auto* omnibox_controller = location_bar_view->GetOmniboxController();
  MockOmniboxEditModel mock_edit_model(omnibox_controller);

  OmniboxPopupFileSelector file_selector(
      browser()->GetWindow()->GetNativeWindow());

  EXPECT_FALSE(presenter->has_active_blockers());

  file_selector.OpenFileUploadDialog(
      browser()->GetTabStripModel()->GetActiveWebContents(),
      /*is_image=*/true, &mock_edit_model, std::nullopt,
      /*was_ai_mode_open=*/false);

  EXPECT_TRUE(presenter->has_active_blockers());

  file_selector.FileSelectionCanceled();

  EXPECT_FALSE(presenter->has_active_blockers());
}
