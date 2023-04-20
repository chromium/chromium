// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"

using storage::FileSystemURL;

namespace ash::cloud_upload {
namespace {

// Returns full test file path to the given |file_name|.
base::FilePath GetTestFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

}  // namespace

// Tests the OneDrive upload workflow using the static
// `OneDriveUploadHandler::Upload` method. Ensures that the upload completes
// with the expected results.
class OneDriveUploadHandlerTest : public InProcessBrowserTest {
 public:
  OneDriveUploadHandlerTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    my_files_dir_ = temp_dir_.GetPath().Append("myfiles");
    test_file_name_ = "text.docx";
  }

  OneDriveUploadHandlerTest(const OneDriveUploadHandlerTest&) = delete;
  OneDriveUploadHandlerTest& operator=(const OneDriveUploadHandlerTest&) =
      delete;

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  // Creates mount point for My files and registers local filesystem.
  void SetUpMyFiles() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    }
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile());
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), my_files_dir_));
  }

  // Creates and mounts fake provided file system for OneDrive.
  void SetUpODFS() {
    file_system_provider::Service* service =
        file_system_provider::Service::Get(profile());
    file_system_provider::MountOptions options("odfs", "ODFS");
    const file_system_provider::ProviderId provider_id =
        file_system_provider::ProviderId::CreateFromExtensionId(
            extension_misc::kODFSExtensionId);
    service->RegisterProvider(
        file_system_provider::FakeExtensionProvider::Create(
            extension_misc::kODFSExtensionId));
    EXPECT_EQ(base::File::FILE_OK,
              service->MountFileSystem(provider_id, options));
  }

  // The exit point of the test. `WaitForUploadComplete` will not complete until
  // this is called.
  void OnUploadDone(const storage::FileSystemURL& uploaded_file_url) {
    ASSERT_TRUE(uploaded_file_url.is_valid());
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void WaitForUploadComplete() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  Profile* profile() { return browser()->profile(); }

  base::FilePath source_file_path() {
    return my_files_dir_.AppendASCII(test_file_name_);
  }

 protected:
  base::FilePath my_files_dir_;
  std::string test_file_name_;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest, UploadToOneDriveSuccess) {
  SetUpMyFiles();
  SetUpODFS();

  // Create test docx file within My files.
  const base::FilePath test_file_path = GetTestFilePath(test_file_name_);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::CopyFile(test_file_path, source_file_path()));
  }

  // Start the upload workflow and end the test once the upload has completed
  // successfully.
  FileSystemURL source_file_url = FilePathToFileSystemURL(
      profile(), file_manager::util::GetFileManagerFileSystemContext(profile()),
      source_file_path());
  OneDriveUploadHandler::Upload(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadDone,
                     base::Unretained(this)));
  WaitForUploadComplete();
}

}  // namespace ash::cloud_upload
