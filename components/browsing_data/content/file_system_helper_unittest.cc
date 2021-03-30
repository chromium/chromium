// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/file_system_helper.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/native_io_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::BrowserThread;

namespace browsing_data {
namespace {

// Shorter names for storage::* constants.
const storage::FileSystemType kTemporary = storage::kFileSystemTypeTemporary;
const storage::FileSystemType kPersistent = storage::kFileSystemTypePersistent;

// TODO(mkwst): Update this size once the discussion in http://crbug.com/86114
// is concluded.
const int kEmptyFileSystemSize = 0;

using FileSystemInfoList = std::list<FileSystemHelper::FileSystemInfo>;

// We'll use these three distinct origins for testing, both as strings and as
// Origins in appropriate contexts.
// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
url::Origin Origin1() {
  return url::Origin::Create(GURL("http://host1:1"));
}
url::Origin Origin2() {
  return url::Origin::Create(GURL("http://host2:2"));
}
url::Origin Origin3() {
  return url::Origin::Create(GURL("http://host3:3"));
}

// Extensions and Devtools should be ignored.
url::Origin OriginExt() {
  return url::Origin::Create(
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz"));
}
url::Origin OriginDevTools() {
  return url::Origin::Create(GURL("devtools://abcdefghijklmnopqrstuvw"));
}

// The FileSystem APIs are all asynchronous; this testing class wraps up the
// boilerplate code necessary to deal with waiting for responses. In a nutshell,
// any async call whose response we want to test ought to create a base::RunLoop
// instance to be followed by a call to BlockUntilQuit(), which will
// (shockingly!) block until Quit() is called on the RunLoop.
class FileSystemHelperTest : public testing::Test {
 public:
  FileSystemHelperTest() {
    auto* file_system_context =
        BrowserContext::GetDefaultStoragePartition(&browser_context_)
            ->GetFileSystemContext();
    auto* native_io_context =
        BrowserContext::GetDefaultStoragePartition(&browser_context_)
            ->GetNativeIOContext();
    helper_ =
        FileSystemHelper::Create(file_system_context, {}, native_io_context);
    content::RunAllTasksUntilIdle();
    canned_helper_ =
        new CannedFileSystemHelper(file_system_context, {}, native_io_context);
  }

  // Blocks on the run_loop quits.
  void BlockUntilQuit(base::RunLoop* run_loop) {
    run_loop->Run();                  // Won't return until Quit().
    content::RunAllTasksUntilIdle();  // Flush other runners.
  }

  // Callback that should be executed in response to
  // storage::FileSystemContext::OpenFileSystem.
  void OpenFileSystemCallback(base::RunLoop* run_loop,
                              const GURL& root,
                              const std::string& name,
                              base::File::Error error) {
    open_file_system_result_ = error;
    run_loop->Quit();
  }

  bool OpenFileSystem(const url::Origin& origin,
                      storage::FileSystemType type,
                      storage::OpenFileSystemMode open_mode) {
    base::RunLoop run_loop;
    BrowserContext::GetDefaultStoragePartition(&browser_context_)
        ->GetFileSystemContext()
        ->OpenFileSystem(
            origin, type, open_mode,
            base::BindOnce(&FileSystemHelperTest::OpenFileSystemCallback,
                           base::Unretained(this), &run_loop));
    BlockUntilQuit(&run_loop);
    return open_file_system_result_ == base::File::FILE_OK;
  }

  // Calls storage::FileSystemContext::OpenFileSystem with
  // OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT flag
  // to verify the existence of a file system for a specified type and origin,
  // blocks until a response is available, then returns the result
  // synchronously to it's caller.
  bool FileSystemContainsOriginAndType(const url::Origin& origin,
                                       storage::FileSystemType type) {
    return OpenFileSystem(origin, type,
                          storage::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT);
  }

  // Callback that should be executed in response to StartFetching(), and stores
  // found file systems locally so that they are available via GetFileSystems().
  void CallbackStartFetching(base::RunLoop* run_loop,
                             const std::list<FileSystemHelper::FileSystemInfo>&
                                 file_system_info_list) {
    file_system_info_list_.reset(
        new std::list<FileSystemHelper::FileSystemInfo>(file_system_info_list));
    run_loop->Quit();
  }

  // Calls StartFetching() on the test's FileSystemHelper
  // object, then blocks until the callback is executed.
  void FetchFileSystems() {
    base::RunLoop run_loop;
    helper_->StartFetching(
        base::BindOnce(&FileSystemHelperTest::CallbackStartFetching,
                       base::Unretained(this), &run_loop));
    BlockUntilQuit(&run_loop);
  }

  // Calls StartFetching() on the test's CannedFileSystemHelper
  // object, then blocks until the callback is executed.
  void FetchCannedFileSystems() {
    base::RunLoop run_loop;
    canned_helper_->StartFetching(
        base::BindOnce(&FileSystemHelperTest::CallbackStartFetching,
                       base::Unretained(this), &run_loop));
    BlockUntilQuit(&run_loop);
  }

  // Sets up Origin1() with a temporary file system, Origin2() with a persistent
  // file system, and Origin3() with both.
  virtual void PopulateTestFileSystemData() {
    CreateDirectoryForOriginAndType(Origin1(), kTemporary);
    CreateDirectoryForOriginAndType(Origin2(), kPersistent);
    CreateDirectoryForOriginAndType(Origin3(), kTemporary);
    CreateDirectoryForOriginAndType(Origin3(), kPersistent);

    EXPECT_FALSE(FileSystemContainsOriginAndType(Origin1(), kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(Origin1(), kTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(Origin2(), kPersistent));
    EXPECT_FALSE(FileSystemContainsOriginAndType(Origin2(), kTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(Origin3(), kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(Origin3(), kTemporary));
  }

  // Calls OpenFileSystem with OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT
  // to create a filesystem of a given type for a specified origin.
  void CreateDirectoryForOriginAndType(const url::Origin& origin,
                                       storage::FileSystemType type) {
    OpenFileSystem(origin, type,
                   storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
    EXPECT_EQ(base::File::FILE_OK, open_file_system_result_);
  }

  // Returns a list of the FileSystemInfo objects gathered in the most recent
  // call to StartFetching().
  FileSystemInfoList* GetFileSystems() { return file_system_info_list_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;

  // Temporary storage to pass information back from callbacks.
  base::File::Error open_file_system_result_;
  std::unique_ptr<FileSystemInfoList> file_system_info_list_;

  scoped_refptr<FileSystemHelper> helper_;
  scoped_refptr<CannedFileSystemHelper> canned_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemHelperTest);
};

// Verifies that the FileSystemHelper correctly finds the test file
// system data, and that each file system returned contains the expected data.
TEST_F(FileSystemHelperTest, FetchData) {
  PopulateTestFileSystemData();

  FetchFileSystems();

  EXPECT_EQ(3UL, file_system_info_list_->size());

  // Order is arbitrary, verify all three origins.
  bool test_hosts_found[3] = {false, false, false};
  for (const auto& info : *file_system_info_list_) {
    if (info.origin == Origin1()) {
      EXPECT_FALSE(test_hosts_found[0]);
      test_hosts_found[0] = true;
      EXPECT_FALSE(base::Contains(info.usage_map, kPersistent));
      EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize,
                info.usage_map.at(storage::kFileSystemTypeTemporary));
    } else if (info.origin == Origin2()) {
      EXPECT_FALSE(test_hosts_found[1]);
      test_hosts_found[1] = true;
      EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
      EXPECT_FALSE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kPersistent));
    } else if (info.origin == Origin3()) {
      EXPECT_FALSE(test_hosts_found[2]);
      test_hosts_found[2] = true;
      EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
      EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kPersistent));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kTemporary));
    } else {
      ADD_FAILURE() << info.origin.Serialize() << " isn't an origin we added.";
    }
  }
  for (size_t i = 0; i < base::size(test_hosts_found); i++) {
    EXPECT_TRUE(test_hosts_found[i]);
  }
}

// Verifies that the FileSystemHelper correctly deletes file
// systems via DeleteFileSystemOrigin().
TEST_F(FileSystemHelperTest, DeleteData) {
  PopulateTestFileSystemData();

  helper_->DeleteFileSystemOrigin(Origin1());
  helper_->DeleteFileSystemOrigin(Origin2());

  FetchFileSystems();

  EXPECT_EQ(1UL, file_system_info_list_->size());
  FileSystemHelper::FileSystemInfo info = *(file_system_info_list_->begin());
  EXPECT_EQ(Origin3(), info.origin);
  EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
  EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_map[kPersistent]);
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_map[kTemporary]);
}

// Verifies that the CannedFileSystemHelper correctly reports
// whether or not it currently contains file systems.
TEST_F(FileSystemHelperTest, Empty) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(Origin1());
  ASSERT_FALSE(canned_helper_->empty());
  canned_helper_->Reset();
  ASSERT_TRUE(canned_helper_->empty());
}

// Verifies that AddFileSystem correctly adds file systems. The canned helper
// does not record usage size.
TEST_F(FileSystemHelperTest, CannedAddFileSystem) {
  canned_helper_->Add(Origin1());
  canned_helper_->Add(Origin2());

  FetchCannedFileSystems();

  EXPECT_EQ(2U, file_system_info_list_->size());
  auto info = file_system_info_list_->begin();
  EXPECT_EQ(Origin1(), info->origin);
  EXPECT_FALSE(base::Contains(info->usage_map, kPersistent));
  EXPECT_FALSE(base::Contains(info->usage_map, kTemporary));

  info++;
  EXPECT_EQ(Origin2(), info->origin);
  EXPECT_FALSE(base::Contains(info->usage_map, kPersistent));
  EXPECT_FALSE(base::Contains(info->usage_map, kTemporary));
}

// Verifies that the CannedFileSystemHelper correctly ignores
// extension and devtools schemes.
TEST_F(FileSystemHelperTest, IgnoreExtensionsAndDevTools) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(OriginExt());
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(OriginDevTools());
  ASSERT_TRUE(canned_helper_->empty());
}

}  // namespace
}  // namespace browsing_data
