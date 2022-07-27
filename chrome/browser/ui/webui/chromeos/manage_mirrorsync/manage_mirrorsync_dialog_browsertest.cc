// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync_dialog.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

using testing::ElementsAre;
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

class ManageMirrorSyncDialogTest : public InProcessBrowserTest {
 public:
  ManageMirrorSyncDialogTest() {
    feature_list_.InitAndEnableFeature(features::kDriveFsMirroring);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ManageMirrorSyncDialogTest(const ManageMirrorSyncDialogTest&) = delete;
  ManageMirrorSyncDialogTest& operator=(const ManageMirrorSyncDialogTest&) =
      delete;

  void SetUpMyFilesAndDialog(std::vector<std::string> paths) {
    // Revoke the existing mount points and setup `temp_dir_` as the mount point
    // for ~/MyFiles.
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(browser()->profile()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        temp_dir_.GetPath());
    my_files_dir_ = temp_dir_.GetPath();

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

    // Show the MirrorSync dialog and wait for it to complete loading.
    content::WebContentsAddedObserver observer;
    ManageMirrorSyncDialog::Show(browser()->profile());
    dialog_contents_ = observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(dialog_contents_));
    EXPECT_EQ(dialog_contents_->GetLastCommittedURL().host(),
              chrome::kChromeUIManageMirrorSyncHost);
  }

  // Helper to send JS to the chrome://manage-mirrorsync dialog and extract it's
  // response. Useful to ensure the API contract the Mojo exposes is valid.
  base::Value::List GetChildFolders(const std::string& path) {
    const std::string js_expression = base::StrCat(
        {"((async () => { "
         "const {BrowserProxy} = await import('./browser_proxy.js');"
         "const handler = BrowserProxy.getInstance().handler;"
         "const {paths} = await handler.getChildFolders({path: '",
         path,
         "'});"
         "return paths; })())"});
    auto response = content::EvalJs(dialog_contents_, js_expression);

    base::Value response_list = response.ExtractList();
    return response_list.GetList().Clone();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_dir_;
  content::WebContents* dialog_contents_;
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
    EXPECT_THAT(child_folders, SizeIs(0));
  }
}

IN_PROC_BROWSER_TEST_F(ManageMirrorSyncDialogTest, InvalidScenarios) {
  SetUpMyFilesAndDialog({{"foo/bar"}, {"baz"}});

  {
    // Not supplying a path should return an empty list.
    const auto& child_folders = GetChildFolders("");
    EXPECT_THAT(child_folders, SizeIs(0));
  }

  {
    // Path traversals should return an empty list.
    const auto& child_folders = GetChildFolders("/../../");
    EXPECT_THAT(child_folders, SizeIs(0));
  }

  {
    // Relative paths should are invalid, empty list is returned.
    const auto& child_folders = GetChildFolders("foo/bar");
    EXPECT_THAT(child_folders, SizeIs(0));
  }

  {
    // Non-existent directories should return an empty list.
    const auto& child_folders = GetChildFolders("/non-existant");
    EXPECT_THAT(child_folders, SizeIs(0));
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
    EXPECT_THAT(child_folders, SizeIs(0));
  }
}

}  // namespace chromeos
