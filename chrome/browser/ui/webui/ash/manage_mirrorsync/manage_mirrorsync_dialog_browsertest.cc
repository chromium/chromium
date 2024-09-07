// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_dialog.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

using base::test::RunOnceCallback;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;

// The Mojo bindings for JS output a `base::FilePath` as an object with a 'path'
// key. When parsing it in C++ as a `base::Value` this requires a bit of
// wrangling. To avoid doing this over and over, this MATCHER unwraps the
// `base::Value` and matches against a supplied `std::vector<std::string>`.
MATCHER_P(MojoFilePaths, matcher, "") {
  std::vector<std::string> paths;
  for (const base::Value& dict_value : arg) {
    const std::string* path_value = dict_value.GetDict().FindString("path");
    paths.push_back(*path_value);
  }
  return testing::ExplainMatchResult(matcher, paths, result_listener);
}

// Matcher to unwrap the `base::Value::Dict` from `getSyncingPaths` and extract
// the "error" key. The value of this is cast into a `GetSyncPathError` to
// compare.
MATCHER_P(SyncPathError, matcher, "") {
  std::optional<int> error = arg.FindInt("error");
  EXPECT_TRUE(error.has_value());
  auto get_sync_path_error =
      static_cast<manage_mirrorsync::mojom::PageHandler::GetSyncPathError>(
          error.value());
  return testing::ExplainMatchResult(matcher, get_sync_path_error,
                                     result_listener);
}

// Matcher to unwrap the `base::Value::Dict` from `getSyncPaths` and extract the
// "syncingPaths" key. This can be coupled wit the `MojoFilePaths` matcher to
// perform element comparison on the resultant `base::Value::Dict` in the array.
MATCHER_P(SyncingPaths, matcher, "") {
  const base::Value::List* paths = arg.FindList("syncingPaths");
  EXPECT_NE(paths, nullptr);
  return testing::ExplainMatchResult(matcher, *paths, result_listener);
}

// Helper to observe the DriveIntegrationService for when mirroring is enabled.
class DriveMirrorSyncStatusObserver
    : public drive::DriveIntegrationService::Observer {
 public:
  explicit DriveMirrorSyncStatusObserver(bool expected_status)
      : expected_status_(expected_status) {
    quit_closure_ = run_loop_.QuitClosure();
  }

  void WaitForStatusChange() { run_loop_.Run(); }

  void OnMirroringEnabled() override {
    quit_closure_.Run();
    EXPECT_TRUE(expected_status_);
  }

  void OnMirroringDisabled() override {
    quit_closure_.Run();
    EXPECT_FALSE(expected_status_);
  }

 private:
  base::RunLoop run_loop_;
  base::RepeatingClosure quit_closure_;
  bool expected_status_ = false;
};

class ManageMirrorSyncDialogTest : public InProcessBrowserTest {
 public:
  ManageMirrorSyncDialogTest() {
    feature_list_.InitAndEnableFeature(features::kDriveFsMirroring);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ManageMirrorSyncDialogTest(const ManageMirrorSyncDialogTest&) = delete;
  ManageMirrorSyncDialogTest& operator=(const ManageMirrorSyncDialogTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &ManageMirrorSyncDialogTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_point = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_point);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, "", mount_point,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  // Show the MirrorSync dialog and wait for it to complete loading.
  void ShowDialog() {
    content::WebContentsAddedObserver observer;
    ManageMirrorSyncDialog::Show(browser()->profile());
    dialog_contents_ = observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(dialog_contents_));
    EXPECT_EQ(dialog_contents_->GetLastCommittedURL().host(),
              chrome::kChromeUIManageMirrorSyncHost);
  }

  void SetUpMyFilesAndDialog(std::vector<std::string> paths) {
    // Revoke the existing mount points and setup `temp_dir_` as the mount point
    // for ~/MyFiles.
    my_files_dir_ = temp_dir_.GetPath().Append("MyFiles");
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(browser()->profile()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        my_files_dir_);

    // Create the supplied paths as individual folders. This ensures a path such
    // as foo/bar gets foo created and bar created as a child folder.
    for (const auto& path : paths) {
      base::FilePath file_path(path);
      const auto& parts = file_path.GetComponents();
      base::FilePath base_path = my_files_dir_;
      for (const auto& path_part : parts) {
        base_path = base_path.Append(path_part);
        base::ScopedAllowBlockingForTesting allow_blocking;
        ASSERT_TRUE(base::CreateDirectory(base_path));
      }
    }

    ShowDialog();
  }

  drivefs::FakeDriveFs& SetUpMirrorSyncAndDialog(bool enabled) {
    // Revoke the existing mount points and setup `temp_dir_` as the mount point
    // for ~/MyFiles.
    my_files_dir_ = temp_dir_.GetPath().Append("MyFiles");
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(browser()->profile()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        my_files_dir_);

    // Turning on MirrorSync requires MyFiles to exist first.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::CreateDirectory(my_files_dir_);
    }

    drivefs::FakeDriveFs& fake_drivefs =
        fake_drivefs_helpers_[browser()->profile()]->fake_drivefs();

    // Toggle the MirrorSync preference to enable / disable the feature.
    {
      DriveMirrorSyncStatusObserver observer(enabled);
      drive::DriveIntegrationService* const service =
          drive::DriveIntegrationServiceFactory::FindForProfile(
              browser()->profile());
      observer.Observe(service);
      // Turning on the sync will add ~/MyFiles as the sync path, which will
      // call GetSyncingPaths internally.
      if (enabled) {
        EXPECT_CALL(fake_drivefs, GetSyncingPaths(_))
            .WillOnce(RunOnceCallback<0>(drive::FileError::FILE_ERROR_OK,
                                         std::vector<base::FilePath>()));
      }
      browser()->profile()->GetPrefs()->SetBoolean(
          drive::prefs::kDriveFsEnableMirrorSync, enabled);
      observer.WaitForStatusChange();
    }

    ShowDialog();

    return fake_drivefs;
  }

  // Returns a pair of std::vector where the first element contains a list of
  // base::FilePaths that represent absolute paths parented at `my_files_dir_`
  // and the second element contains the expected paths returned from
  // `getSyncingPaths` i.e. with the leading `my_files_dir_` replaced with a "/"
  // character. For example: CreateExpectedPaths({{"foo"}}) will have the
  // following first element:
  //   /MyFiles/foo
  // And the second element will be:
  //   /foo
  std::pair<std::vector<base::FilePath>, std::vector<std::string>>
  CreateExpectedPaths(std::vector<std::string> paths) {
    std::vector<base::FilePath> absolute_paths;
    std::vector<std::string> expected_paths;
    for (const auto& path : paths) {
      absolute_paths.push_back(my_files_dir_.Append(path));
      expected_paths.push_back(base::StrCat({"/", path}));
    }
    return std::make_pair(absolute_paths, expected_paths);
  }

  // Helper to invoke the `getChildFolders` method on chrome://manage-mirrorsync
  // dialog and extract it's response.
  base::Value::List GetChildFolders(const std::string& path) {
    const std::string js_expression = base::StrCat(
        {"((async () => { "
         "const {BrowserProxy} = await import('./browser_proxy.js');"
         "const handler = BrowserProxy.getInstance().handler;"
         "const {paths} = await handler.getChildFolders({path: '",
         path,
         "'});"
         "return paths; })())"});
    auto response = content::EvalJs(dialog_contents_.get(), js_expression);

    base::Value response_list = response.ExtractList();
    return response_list.GetList().Clone();
  }

  // Helper to invoke the `getSyncingPaths` method on chrome://manage-mirrorsync
  // dialog and extract it's response.
  base::Value::Dict GetSyncingPaths() {
    const std::string js_expression =
        "((async () => { "
        "const {BrowserProxy} = await import('./browser_proxy.js');"
        "const handler = BrowserProxy.getInstance().handler;"
        "const response = await handler.getSyncingPaths();"
        "return response; })())";
    auto response = content::EvalJs(dialog_contents_.get(), js_expression);
    EXPECT_TRUE(response.value.is_dict());
    return response.value.GetDict().Clone();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_dir_;
  raw_ptr<content::WebContents, DanglingUntriaged> dialog_contents_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
};

IN_PROC_BROWSER_TEST_F(ManageMirrorSyncDialogTest, ExpectedScenarios) {
  SetUpMyFilesAndDialog({{"foo/bar"}, {"baz"}});
  {
    // Only children folders should be returned (i.e. not all descendants).
    const auto& child_folders = GetChildFolders("/");
    EXPECT_THAT(child_folders, SizeIs(2));
    EXPECT_THAT(child_folders, MojoFilePaths(ElementsAre("/baz", "/foo")));
  }

  {
    // Paths should be returned as absolute.
    const auto& child_folders = GetChildFolders("/foo");
    EXPECT_THAT(child_folders, SizeIs(1));
    EXPECT_THAT(child_folders, MojoFilePaths(ElementsAre("/foo/bar")));
  }

  {
    // Folders with no children should return an empty list.
    const auto& child_folders = GetChildFolders("/baz");
    EXPECT_THAT(child_folders, IsEmpty());
  }
}

IN_PROC_BROWSER_TEST_F(ManageMirrorSyncDialogTest, InvalidScenarios) {
  SetUpMyFilesAndDialog({{"foo/bar"}, {"baz"}});

  {
    // Not supplying a path should return an empty list.
    const auto& child_folders = GetChildFolders("");
    EXPECT_THAT(child_folders, IsEmpty());
  }

  {
    // Path traversals should return an empty list.
    const auto& child_folders = GetChildFolders("/../../");
    EXPECT_THAT(child_folders, IsEmpty());
  }

  {
    // Relative paths should are invalid, empty list is returned.
    const auto& child_folders = GetChildFolders("foo/bar");
    EXPECT_THAT(child_folders, IsEmpty());
  }

  {
    // Non-existent directories should return an empty list.
    const auto& child_folders = GetChildFolders("/non-existant");
    EXPECT_THAT(child_folders, IsEmpty());
  }
}

IN_PROC_BROWSER_TEST_F(ManageMirrorSyncDialogTest, CertainItemsAreIgnored) {
  SetUpMyFilesAndDialog({{"foo/bar"}, {"baz"}});

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Create a file under foo, files should be ignored.
    ASSERT_TRUE(base::WriteFile(my_files_dir_.Append("foo/test.txt"), "blah"));

    // Any items (files or folders) with a leading dot should be ignored.
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_.Append("baz/.Trash")));

    // Items nested in dot folders should also be ignored.
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_.Append("baz/.Trash/dir")));
  }

  {
    // File should be ignored as well as any dot folders.
    const auto& child_folders = GetChildFolders("/");
    EXPECT_THAT(child_folders, SizeIs(2));
    EXPECT_THAT(child_folders, MojoFilePaths(ElementsAre("/baz", "/foo")));
  }

  {
    // Supplied dot folders should return an empty list.
    const auto& child_folders = GetChildFolders("/baz/.Trash");
    EXPECT_THAT(child_folders, IsEmpty());
  }
}

using manage_mirrorsync::mojom::PageHandler;

IN_PROC_BROWSER_TEST_F(ManageMirrorSyncDialogTest,
                       GetSyncingPathsMirrorSyncEnabled) {
  drivefs::FakeDriveFs& fake_drivefs =
      SetUpMirrorSyncAndDialog(/*enabled=*/true);

  {
    // No paths currently syncing returns empty.
    const auto& [mock_paths, expected_paths] = CreateExpectedPaths({});
    EXPECT_CALL(fake_drivefs, GetSyncingPaths(_))
        .WillOnce(
            RunOnceCallback<0>(drive::FileError::FILE_ERROR_OK, mock_paths));
    const auto& syncing_paths = GetSyncingPaths();
    EXPECT_THAT(syncing_paths,
                AllOf(SyncPathError(PageHandler::GetSyncPathError::kSuccess),
                      SyncingPaths(IsEmpty())));
  }

  {
    // Single path being synced returns correctly.
    const auto& [mock_paths, expected_paths] = CreateExpectedPaths({{"foo"}});
    EXPECT_CALL(fake_drivefs, GetSyncingPaths(_))
        .WillOnce(
            RunOnceCallback<0>(drive::FileError::FILE_ERROR_OK, mock_paths));
    const auto& syncing_paths = GetSyncingPaths();
    EXPECT_THAT(
        syncing_paths,
        AllOf(SyncPathError(PageHandler::GetSyncPathError::kSuccess),
              SyncingPaths(MojoFilePaths(ElementsAreArray(expected_paths)))));
  }

  {
    // Nested syncing path returns itself but not the parents.
    const auto& [mock_paths, expected_paths] =
        CreateExpectedPaths({{"foo/bar"}});
    EXPECT_CALL(fake_drivefs, GetSyncingPaths(_))
        .WillOnce(
            RunOnceCallback<0>(drive::FileError::FILE_ERROR_OK, mock_paths));
    const auto& syncing_paths = GetSyncingPaths();
    EXPECT_THAT(
        syncing_paths,
        AllOf(SyncPathError(PageHandler::GetSyncPathError::kSuccess),
              SyncingPaths(MojoFilePaths(ElementsAreArray(expected_paths)))));
  }

  {
    // Failure from retrieval of paths should return error + empty vector.
    const auto& [mock_paths, expected_paths] = CreateExpectedPaths({});
    EXPECT_CALL(fake_drivefs, GetSyncingPaths(_))
        .WillOnce(RunOnceCallback<0>(drive::FileError::FILE_ERROR_NOT_FOUND,
                                     mock_paths));
    const auto& syncing_paths = GetSyncingPaths();
    EXPECT_THAT(syncing_paths,
                AllOf(SyncPathError(PageHandler::GetSyncPathError::kFailed),
                      SyncingPaths(IsEmpty())));
  }

  {
    // Paths returned not parented at MyFiles should be ignored.
    auto [mock_paths, expected_paths] = CreateExpectedPaths({{"baz"}});
    mock_paths.push_back(base::FilePath("/ignore/dir"));
    EXPECT_CALL(fake_drivefs, GetSyncingPaths(_))
        .WillOnce(
            RunOnceCallback<0>(drive::FileError::FILE_ERROR_OK, mock_paths));
    const auto& syncing_paths = GetSyncingPaths();
    EXPECT_THAT(
        syncing_paths,
        AllOf(SyncPathError(PageHandler::GetSyncPathError::kSuccess),
              SyncingPaths(AllOf(SizeIs(1), MojoFilePaths(ElementsAreArray(
                                                expected_paths))))));
  }
}

IN_PROC_BROWSER_TEST_F(ManageMirrorSyncDialogTest,
                       GetSyncingPathsMirrorSyncDisabled) {
  SetUpMirrorSyncAndDialog(/*enabled=*/false);

  {
    // MirrorSync not enabled should return kServiceUnavailable and empty paths.
    const auto& syncing_paths = GetSyncingPaths();
    EXPECT_THAT(
        syncing_paths,
        AllOf(SyncPathError(PageHandler::GetSyncPathError::kServiceUnavailable),
              SyncingPaths(IsEmpty())));
  }
}

}  // namespace ash
