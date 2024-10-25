// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/screen_ai/public/test/fake_optical_character_recognizer.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

class ChromeCaptureModeDelegateBrowserTest
    : public InProcessBrowserTest,
      public screen_ai::ScreenAIInstallState::Observer {
 public:
  ChromeCaptureModeDelegateBrowserTest() = default;
  ChromeCaptureModeDelegateBrowserTest(
      const ChromeCaptureModeDelegateBrowserTest&) = delete;
  ChromeCaptureModeDelegateBrowserTest& operator=(
      const ChromeCaptureModeDelegateBrowserTest&) = delete;
  ~ChromeCaptureModeDelegateBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    screen_ai_install_state_observer_.Observe(
        screen_ai::ScreenAIInstallState::GetInstance());
  }

  void TearDownOnMainThread() override {
    // Reset the screen ai install state observer before browser shut down and
    // destruction of the screen_ai::ScreenAIInstallState.
    screen_ai_install_state_observer_.Reset();
  }

  // screen_ai::ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override {
    if (state == screen_ai::ScreenAIInstallState::State::kDownloading) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            screen_ai::ScreenAIInstallState::GetInstance()->SetState(
                screen_ai::ScreenAIInstallState::State::kDownloadFailed);
          }));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kScannerUpdate};

  // The OCR service is not supported on ChromeOS browser tests, so use an
  // observer to fail the download when requested. Otherwise, the download
  // request will time out.
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      screen_ai_install_state_observer_{this};
};

IN_PROC_BROWSER_TEST_F(ChromeCaptureModeDelegateBrowserTest,
                       FileNotRedirected) {
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Create regular file in downloads.
  const base::FilePath downloads_path =
      delegate->GetUserDefaultDownloadsFolder();
  base::FilePath path;
  base::CreateTemporaryFileInDir(downloads_path, &path);

  // Should not be redirected.
  EXPECT_EQ(path, delegate->RedirectFilePath(path));

  // Successfully finalized to the same location.
  base::test::TestFuture<bool, const base::FilePath&> path_future;
  delegate->FinalizeSavedFile(path_future.GetCallback(), path, gfx::Image());
  EXPECT_TRUE(path_future.Get<0>());
  EXPECT_EQ(path_future.Get<1>(), path);

  // Cleanup.
  EXPECT_TRUE(base::PathExists(path));
  base::DeleteFile(path);
}

IN_PROC_BROWSER_TEST_F(ChromeCaptureModeDelegateBrowserTest,
                       OdfsFileRedirected) {
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Mount ODFS.
  file_manager::test::FakeProvidedFileSystemOneDrive* provided_file_system =
      file_manager::test::MountFakeProvidedFileSystemOneDrive(
          browser()->profile());
  ASSERT_TRUE(provided_file_system);
  EXPECT_FALSE(delegate->GetOneDriveMountPointPath().empty());

  // Check that file going to OneDrive will be redirected to /tmp.
  const std::string test_file_name = "capture_mode_delegate.test";
  base::FilePath original_file =
      delegate->GetOneDriveMountPointPath().Append(test_file_name);
  base::FilePath redirected_path = delegate->RedirectFilePath(original_file);
  EXPECT_NE(redirected_path, original_file);
  base::FilePath tmp_dir;
  ASSERT_TRUE(base::GetTempDir(&tmp_dir));
  EXPECT_TRUE(tmp_dir.IsParent(redirected_path));

  // Create the redirected file.
  base::File file(redirected_path,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  file.Close();

  // Check that the file is successfully finalized to different location.
  base::test::TestFuture<bool, const base::FilePath&> path_future;
  delegate->FinalizeSavedFile(path_future.GetCallback(), redirected_path,
                              gfx::Image());
  EXPECT_TRUE(path_future.Get<0>());

  // Check that file now exists in OneDrive.
  base::test::TestFuture<
      std::unique_ptr<ash::file_system_provider::EntryMetadata>,
      base::File::Error>
      metadata_future;
  provided_file_system->GetMetadata(base::FilePath("/").Append(test_file_name),
                                    {}, metadata_future.GetCallback());
  EXPECT_EQ(base::File::Error::FILE_OK,
            metadata_future.Get<base::File::Error>());

  // Original file was moved.
  EXPECT_FALSE(base::PathExists(redirected_path));
}

// The OCR service is not supported on ChromeOS browser tests, so we can't check
// the real detected text.
IN_PROC_BROWSER_TEST_F(ChromeCaptureModeDelegateBrowserTest,
                       EmptyDetectedTextWhenOCRNotSupported) {
  base::test::TestFuture<std::string> detected_text_future;

  ChromeCaptureModeDelegate::Get()->DetectTextInImage(
      SkBitmap(), detected_text_future.GetCallback());

  EXPECT_EQ(detected_text_future.Get(), "");
}

// Simulates successful text detection using a fake OCR backend.
IN_PROC_BROWSER_TEST_F(ChromeCaptureModeDelegateBrowserTest,
                       DetectsTextWhenOCRSupported) {
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  scoped_refptr<screen_ai::FakeOpticalCharacterRecognizer>
      optical_character_recognizer =
          screen_ai::FakeOpticalCharacterRecognizer::Create(
              /*empty_ax_tree_update_result=*/false);
  auto visual_annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line1 = screen_ai::mojom::LineBox::New();
  line1->text_line = "Text";
  visual_annotation->lines.push_back(std::move(line1));
  auto line2 = screen_ai::mojom::LineBox::New();
  line2->text_line = "😊";
  visual_annotation->lines.push_back(std::move(line2));
  optical_character_recognizer->set_visual_annotation_result(
      std::move(visual_annotation));
  delegate->set_optical_character_recognizer_for_testing(
      std::move(optical_character_recognizer));
  base::test::TestFuture<std::string> detected_text_future;

  delegate->DetectTextInImage(SkBitmap(), detected_text_future.GetCallback());

  EXPECT_EQ(detected_text_future.Get(), "Text\n😊");
}

IN_PROC_BROWSER_TEST_F(ChromeCaptureModeDelegateBrowserTest,
                       SessionClosedDuringTextDetection) {
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  delegate->set_optical_character_recognizer_for_testing(
      screen_ai::FakeOpticalCharacterRecognizer::Create(
          /*empty_ax_tree_update_result=*/false));
  base::test::TestFuture<std::string> detected_text_future;

  // Close the session immediately after a text detection request.
  delegate->DetectTextInImage(SkBitmap(), detected_text_future.GetCallback());
  delegate->OnSessionStateChanged(/*started=*/false);

  EXPECT_EQ(detected_text_future.Get(), "");
}
