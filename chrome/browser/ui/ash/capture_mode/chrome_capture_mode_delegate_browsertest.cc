// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ChromeCaptureModeDelegateBrowserTest = InProcessBrowserTest;

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
