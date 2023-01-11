// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/file_system_helper.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

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

// The FileSystem APIs are all asynchronous; this testing class wraps up the
// boilerplate code necessary to deal with waiting for responses. In a nutshell,
// any async call whose response we want to test ought to create a base::RunLoop
// instance to be followed by a call to BlockUntilQuit(), which will
// (shockingly!) block until Quit() is called on the RunLoop.
class FileSystemHelperTest : public testing::Test {
 public:
  FileSystemHelperTest() {
    auto* file_system_context =
        browser_context_.GetDefaultStoragePartition()->GetFileSystemContext();
    helper_ = base::MakeRefCounted<FileSystemHelper>(
        file_system_context, std::vector<storage::FileSystemType>());
    content::RunAllTasksUntilIdle();
    canned_helper_ = base::MakeRefCounted<CannedFileSystemHelper>(
        file_system_context, std::vector<storage::FileSystemType>());
  }

  FileSystemHelperTest(const FileSystemHelperTest&) = delete;
  FileSystemHelperTest& operator=(const FileSystemHelperTest&) = delete;

  // Blocks on the run_loop quits.
  void BlockUntilQuit(base::RunLoop* run_loop) {
    run_loop->Run();                  // Won't return until Quit().
    content::RunAllTasksUntilIdle();  // Flush other runners.
  }

  // Callback that should be executed in response to
  // storage::FileSystemContext::OpenFileSystem.
  void OpenFileSystemCallback(base::RunLoop* run_loop,
                              const storage::FileSystemURL& root,
                              const std::string& name,
                              base::File::Error error) {
    open_file_system_result_ = error;
    run_loop->Quit();
  }

  bool OpenFileSystem(const url::Origin& origin,
                      storage::FileSystemType type,
                      storage::OpenFileSystemMode open_mode) {
    base::RunLoop run_loop;
    browser_context_.GetDefaultStoragePartition()
        ->GetFileSystemContext()
        ->OpenFileSystem(
            blink::StorageKey(origin), /*bucket=*/absl::nullopt, type,
            open_mode,
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
    file_system_info_list_ =
        std::make_unique<std::list<FileSystemHelper::FileSystemInfo>>(
            file_system_info_list);
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

  // Sets up |origin1| with a temporary file system, |origin2| with a persistent
  // file system, and |origin3| with both.
  virtual void PopulateTestFileSystemData(const url::Origin& origin1,
                                          const url::Origin& origin2,
                                          const url::Origin& origin3) {
    CreateDirectoryForOriginAndType(origin1, kTemporary);
    EXPECT_FALSE(FileSystemContainsOriginAndType(origin1, kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(origin1, kTemporary));

    CreateDirectoryForOriginAndType(origin2, kPersistent);
    EXPECT_TRUE(FileSystemContainsOriginAndType(origin2, kPersistent));
    EXPECT_FALSE(FileSystemContainsOriginAndType(origin2, kTemporary));

    CreateDirectoryForOriginAndType(origin3, kTemporary);
    CreateDirectoryForOriginAndType(origin3, kPersistent);
    EXPECT_TRUE(FileSystemContainsOriginAndType(origin3, kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(origin3, kTemporary));
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
};

// Verifies that the FileSystemHelper correctly finds the test file
// system data, and that each file system returned contains the expected data.
TEST_F(FileSystemHelperTest, FetchData) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:1"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://host2:2"));
  const url::Origin origin3 = url::Origin::Create(GURL("http://host3:3"));
  PopulateTestFileSystemData(origin1, origin2, origin3);

  FetchFileSystems();

  EXPECT_EQ(3u, file_system_info_list_->size());

  // Order is arbitrary, verify all three origins.
  bool test_hosts_found[3] = {false, false, false};
  for (const auto& info : *file_system_info_list_) {
    if (info.origin == origin1) {
      EXPECT_FALSE(test_hosts_found[0]);
      test_hosts_found[0] = true;
      EXPECT_FALSE(base::Contains(info.usage_map, kPersistent));
      EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize,
                info.usage_map.at(storage::kFileSystemTypeTemporary));
    } else if (info.origin == origin2) {
      EXPECT_FALSE(test_hosts_found[1]);
      test_hosts_found[1] = true;
      EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
      EXPECT_FALSE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kPersistent));
    } else if (info.origin == origin3) {
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
  for (const auto found : test_hosts_found) {
    EXPECT_TRUE(found);
  }
}

// Verifies that the FileSystemHelper correctly deletes file
// systems via DeleteFileSystemOrigin().
TEST_F(FileSystemHelperTest, DeleteData) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:1"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://host2:2"));
  const url::Origin origin3 = url::Origin::Create(GURL("http://host3:3"));
  PopulateTestFileSystemData(origin1, origin2, origin3);

  helper_->DeleteFileSystemOrigin(origin1);
  helper_->DeleteFileSystemOrigin(origin2);

  FetchFileSystems();

  EXPECT_EQ(1UL, file_system_info_list_->size());
  FileSystemHelper::FileSystemInfo info = file_system_info_list_->front();
  EXPECT_EQ(origin3, info.origin);
  EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
  EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_map[kPersistent]);
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_map[kTemporary]);
}

// Verifies that the CannedFileSystemHelper correctly reports
// whether or not it currently contains file systems.
TEST_F(FileSystemHelperTest, Empty) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(url::Origin::Create(GURL("http://host1:1")));
  ASSERT_FALSE(canned_helper_->empty());
  canned_helper_->Reset();
  ASSERT_TRUE(canned_helper_->empty());
}

// Verifies that AddFileSystem correctly adds file systems. The canned helper
// does not record usage size.
TEST_F(FileSystemHelperTest, CannedAddFileSystem) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:1"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://host2:2"));
  canned_helper_->Add(origin1);
  canned_helper_->Add(origin2);

  FetchCannedFileSystems();

  EXPECT_EQ(2U, file_system_info_list_->size());
  auto info = file_system_info_list_->begin();
  EXPECT_EQ(origin1, info->origin);
  EXPECT_FALSE(base::Contains(info->usage_map, kPersistent));
  EXPECT_FALSE(base::Contains(info->usage_map, kTemporary));

  ++info;
  EXPECT_EQ(origin2, info->origin);
  EXPECT_FALSE(base::Contains(info->usage_map, kPersistent));
  EXPECT_FALSE(base::Contains(info->usage_map, kTemporary));
}

// Verifies that the CannedFileSystemHelper correctly ignores
// extension and devtools schemes.
TEST_F(FileSystemHelperTest, IgnoreExtensionsAndDevTools) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(url::Origin::Create(
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz")));
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(
      url::Origin::Create(GURL("devtools://abcdefghijklmnopqrstuvw")));
  ASSERT_TRUE(canned_helper_->empty());
}

}  // namespace
}  // namespace browsing_data
