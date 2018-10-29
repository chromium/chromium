// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/resource_metadata.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/fake_free_disk_space_getter.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/drive.pb.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {
namespace {

// The start page token of the resource metadata used in ResourceMetadataTest.
constexpr char kTestStartPageToken[] = "a token";

// Returns the sorted base names from |entries|.
std::vector<std::string> GetSortedBaseNames(
    const ResourceEntryVector& entries) {
  std::vector<std::string> base_names;
  for (size_t i = 0; i < entries.size(); ++i)
    base_names.push_back(entries[i].base_name());
  std::sort(base_names.begin(), base_names.end());

  return base_names;
}

// Creates a ResourceEntry for a directory with explicitly set resource_id.
ResourceEntry CreateDirectoryEntryWithResourceId(
    const std::string& title,
    const std::string& resource_id,
    const std::string& parent_local_id) {
  ResourceEntry entry;
  entry.set_title(title);
  entry.set_resource_id(resource_id);
  entry.set_parent_local_id(parent_local_id);
  entry.mutable_file_info()->set_is_directory(true);
  entry.mutable_directory_specific_info()->set_start_page_token(
      kTestStartPageToken);
  return entry;
}

// Creates a ResourceEntry for a directory.
ResourceEntry CreateDirectoryEntry(const std::string& title,
                                   const std::string& parent_local_id) {
  return CreateDirectoryEntryWithResourceId(
      title, "id:" + title, parent_local_id);
}

// Creates a ResourceEntry for a file with explicitly set resource_id.
ResourceEntry CreateFileEntryWithResourceId(
    const std::string& title,
    const std::string& resource_id,
    const std::string& parent_local_id) {
  ResourceEntry entry;
  entry.set_title(title);
  entry.set_resource_id(resource_id);
  entry.set_parent_local_id(parent_local_id);
  entry.mutable_file_info()->set_is_directory(false);
  entry.mutable_file_info()->set_size(1024);
  entry.mutable_file_specific_info()->set_md5("md5:" + title);
  return entry;
}

// Creates a ResourceEntry for a file.
ResourceEntry CreateFileEntry(const std::string& title,
                              const std::string& parent_local_id) {
  return CreateFileEntryWithResourceId(title, "id:" + title, parent_local_id);
}

// Creates the following files/directories
// drive/root/dir1/
// drive/root/dir2/
// drive/root/dir1/dir3/
// drive/root/dir1/file4
// drive/root/dir1/file5
// drive/root/dir2/file6
// drive/root/dir2/file7
// drive/root/dir2/file8
// drive/root/dir1/dir3/file9
// drive/root/dir1/dir3/file10
void SetUpEntries(ResourceMetadata* resource_metadata) {
  std::string local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->GetIdByPath(
      util::GetDriveMyDriveRootPath(), &local_id));
  const std::string root_local_id = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateDirectoryEntry("dir1", root_local_id), &local_id));
  const std::string local_id_dir1 = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateDirectoryEntry("dir2", root_local_id), &local_id));
  const std::string local_id_dir2 = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateDirectoryEntry("dir3", local_id_dir1), &local_id));
  const std::string local_id_dir3 = local_id;

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file4", local_id_dir1), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file5", local_id_dir1), &local_id));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file6", local_id_dir2), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file7", local_id_dir2), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file8", local_id_dir2), &local_id));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file9", local_id_dir3), &local_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(
      CreateFileEntry("file10", local_id_dir3), &local_id));

  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata->SetStartPageToken(kTestStartPageToken));
}

}  // namespace

// Tests for methods running on the blocking task runner.
class ResourceMetadataTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    fake_free_disk_space_getter_ = std::make_unique<FakeFreeDiskSpaceGetter>();
    cache_.reset(new FileCache(metadata_storage_.get(), temp_dir_.GetPath(),
                               base::ThreadTaskRunnerHandle::Get().get(),
                               fake_free_disk_space_getter_.get()));
    ASSERT_TRUE(cache_->Initialize());

    resource_metadata_.reset(new ResourceMetadata(
        metadata_storage_.get(), cache_.get(),
        base::ThreadTaskRunnerHandle::Get()));

    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->Initialize());

    SetUpEntries(resource_metadata_.get());
  }

  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  std::unique_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  std::unique_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  std::unique_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
};

TEST_F(ResourceMetadataTest, StartPageToken) {
  constexpr char kStartPageToken[] = "1234567";
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->SetStartPageToken(kStartPageToken));
  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ(kStartPageToken, start_page_token);
}

TEST_F(ResourceMetadataTest, GetResourceEntryByPath) {
  // Confirm that an existing file is found.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &entry));
  EXPECT_EQ("file4", entry.base_name());

  // Confirm that a non existing file is not found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/non_existing"), &entry));

  // Confirm that the root is found.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive"), &entry));

  // Confirm that a non existing file is not found at the root level.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("non_existing"), &entry));

  // Confirm that an entry is not found with a wrong root.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("non_existing/root"), &entry));
}

TEST_F(ResourceMetadataTest, ReadDirectoryByPath) {
  // Confirm that an existing directory is found.
  ResourceEntryVector entries;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"), &entries));
  ASSERT_EQ(3U, entries.size());
  // The order is not guaranteed so we should sort the base names.
  std::vector<std::string> base_names = GetSortedBaseNames(entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Confirm that a non existing directory is not found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/non_existing"), &entries));

  // Confirm that reading a file results in FILE_ERROR_NOT_A_DIRECTORY.
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &entries));
}

TEST_F(ResourceMetadataTest, RefreshEntry) {
  base::FilePath drive_file_path;
  ResourceEntry entry;

  // Get file9.
  std::string file_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"), &file_id));
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(file_id, &entry));
  EXPECT_EQ("file9", entry.base_name());
  EXPECT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ("md5:file9", entry.file_specific_info().md5());

  // Rename it.
  ResourceEntry file_entry(entry);
  file_entry.set_title("file100");
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->RefreshEntry(file_entry));

  base::FilePath path;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetFilePath(file_id, &path));
  EXPECT_EQ("drive/root/dir1/dir3/file100", path.AsUTF8Unsafe());
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(file_id, &entry));
  EXPECT_EQ("file100", entry.base_name());
  EXPECT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ("md5:file9", entry.file_specific_info().md5());

  // Update the file md5.
  const std::string updated_md5("md5:updated");
  file_entry = entry;
  file_entry.mutable_file_specific_info()->set_md5(updated_md5);
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->RefreshEntry(file_entry));

  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetFilePath(file_id, &path));
  EXPECT_EQ("drive/root/dir1/dir3/file100", path.AsUTF8Unsafe());
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(file_id, &entry));
  EXPECT_EQ("file100", entry.base_name());
  EXPECT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ(updated_md5, entry.file_specific_info().md5());

  // Make sure we get the same thing from GetResourceEntryByPath.
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file100"), &entry));
  EXPECT_EQ("file100", entry.base_name());
  ASSERT_TRUE(!entry.file_info().is_directory());
  EXPECT_EQ(updated_md5, entry.file_specific_info().md5());

  // Get dir2.
  entry.Clear();
  std::string dir_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"), &dir_id));
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(dir_id, &entry));
  EXPECT_EQ("dir2", entry.base_name());
  ASSERT_TRUE(entry.file_info().is_directory());

  // Get dir3's ID.
  std::string dir3_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"), &dir3_id));

  // Change the name to dir100 and change the parent to drive/dir1/dir3.
  ResourceEntry dir_entry(entry);
  dir_entry.set_title("dir100");
  dir_entry.set_parent_local_id(dir3_id);
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RefreshEntry(dir_entry));

  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetFilePath(dir_id, &path));
  EXPECT_EQ("drive/root/dir1/dir3/dir100", path.AsUTF8Unsafe());
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryById(dir_id, &entry));
  EXPECT_EQ("dir100", entry.base_name());
  EXPECT_TRUE(entry.file_info().is_directory());
  EXPECT_EQ("id:dir2", entry.resource_id());

  // Make sure the children have moved over. Test file6.
  entry.Clear();
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/dir100/file6"),
      &entry));
  EXPECT_EQ("file6", entry.base_name());

  // Make sure dir2 no longer exists.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"), &entry));

  // Make sure that directory cannot move under a file.
  dir_entry.set_parent_local_id(file_id);
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY,
            resource_metadata_->RefreshEntry(dir_entry));

  // Cannot refresh root.
  dir_entry.Clear();
  dir_entry.set_local_id(util::kDriveGrandRootLocalId);
  dir_entry.set_title("new-root-name");
  dir_entry.set_parent_local_id(dir3_id);
  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION,
            resource_metadata_->RefreshEntry(dir_entry));
}

TEST_F(ResourceMetadataTest, RefreshEntry_ResourceIDCheck) {
  // Get an entry with a non-empty resource ID.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"), &entry));
  EXPECT_FALSE(entry.resource_id().empty());

  // Add a new entry with an empty resource ID.
  ResourceEntry new_entry;
  new_entry.set_parent_local_id(entry.local_id());
  new_entry.set_title("new entry");
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(new_entry, &local_id));

  // Try to refresh the new entry with a used resource ID.
  new_entry.set_local_id(local_id);
  new_entry.set_resource_id(entry.resource_id());
  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION,
            resource_metadata_->RefreshEntry(new_entry));
}

TEST_F(ResourceMetadataTest, RefreshEntry_DoNotOverwriteCacheState) {
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &entry));

  // Try to set MD5 with RefreshEntry.
  entry.mutable_file_specific_info()->mutable_cache_state()->set_md5("md5");
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RefreshEntry(entry));

  // Cache state is unchanged.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().md5().empty());

  // Pin the file.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(entry.local_id()));

  // Try to clear the cache state with RefreshEntry.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &entry));
  entry.mutable_file_specific_info()->clear_cache_state();
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RefreshEntry(entry));

  // Cache state is not cleared.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_pinned());
}

TEST_F(ResourceMetadataTest, GetSubDirectoriesRecursively) {
  std::set<base::FilePath> sub_directories;

  // file9: not a directory, so no children.
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetSubDirectoriesRecursively(
      local_id, &sub_directories));
  EXPECT_TRUE(sub_directories.empty());

  // dir2: no child directories.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir2"), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetSubDirectoriesRecursively(
      local_id, &sub_directories));
  EXPECT_TRUE(sub_directories.empty());
  const std::string dir2_id = local_id;

  // dir1: dir3 is the only child
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetSubDirectoriesRecursively(
      local_id, &sub_directories));
  EXPECT_EQ(1u, sub_directories.size());
  EXPECT_EQ(1u, sub_directories.count(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3")));
  sub_directories.clear();

  // Add a few more directories to make sure deeper nesting works.
  // dir2/dir100
  // dir2/dir101
  // dir2/dir101/dir102
  // dir2/dir101/dir103
  // dir2/dir101/dir104
  // dir2/dir101/dir104/dir105
  // dir2/dir101/dir104/dir105/dir106
  // dir2/dir101/dir104/dir105/dir106/dir107
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir100", dir2_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir101", dir2_id), &local_id));
  const std::string dir101_id = local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir102", dir101_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir103", dir101_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir104", dir101_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir105", local_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir106", local_id), &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("dir107", local_id), &local_id));

  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetSubDirectoriesRecursively(
      dir2_id, &sub_directories));
  EXPECT_EQ(8u, sub_directories.size());
  EXPECT_EQ(1u, sub_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101")));
  EXPECT_EQ(1u, sub_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101/dir104")));
  EXPECT_EQ(1u, sub_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/dir2/dir101/dir104/dir105/dir106/dir107")));
}

TEST_F(ResourceMetadataTest, AddEntry) {
  // Add a file to dir3.
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"), &local_id));
  ResourceEntry file_entry = CreateFileEntry("file100", local_id);
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(file_entry, &local_id));
  base::FilePath path;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetFilePath(local_id, &path));
  EXPECT_EQ("drive/root/dir1/dir3/file100", path.AsUTF8Unsafe());

  // Add a directory.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1"), &local_id));
  ResourceEntry dir_entry = CreateDirectoryEntry("dir101", local_id);
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(dir_entry, &local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetFilePath(local_id, &path));
  EXPECT_EQ("drive/root/dir1/dir101", path.AsUTF8Unsafe());

  // Add to an invalid parent.
  ResourceEntry file_entry3 = CreateFileEntry("file103", "id:invalid");
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            resource_metadata_->AddEntry(file_entry3, &local_id));

  // Add an existing file.
  EXPECT_EQ(FILE_ERROR_EXISTS,
            resource_metadata_->AddEntry(file_entry, &local_id));
}

TEST_F(ResourceMetadataTest, RemoveEntry) {
  // Make sure file9 is found.
  std::string file9_local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3/file9"),
      &file9_local_id));
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file9_local_id, &entry));
  EXPECT_EQ("file9", entry.base_name());

  // Remove file9.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RemoveEntry(file9_local_id));

  // file9 should no longer exist.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryById(
      file9_local_id, &entry));

  // Look for dir3.
  std::string dir3_local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/dir3"), &dir3_local_id));
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir3_local_id, &entry));
  EXPECT_EQ("dir3", entry.base_name());

  // Remove dir3.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->RemoveEntry(dir3_local_id));

  // dir3 should no longer exist.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryById(
      dir3_local_id, &entry));

  // Remove unknown local_id using RemoveEntry.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->RemoveEntry("foo"));

  // Try removing root. This should fail.
  EXPECT_EQ(FILE_ERROR_ACCESS_DENIED, resource_metadata_->RemoveEntry(
      util::kDriveGrandRootLocalId));
}

TEST_F(ResourceMetadataTest, GetResourceEntryById_RootDirectory) {
  // Look up the root directory by its ID.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      util::kDriveGrandRootLocalId, &entry));
  EXPECT_EQ("drive", entry.base_name());
}

TEST_F(ResourceMetadataTest, GetResourceEntryById) {
  // Get file4 by path.
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/dir1/file4"), &local_id));

  // Confirm that an existing file is found.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      local_id, &entry));
  EXPECT_EQ("file4", entry.base_name());

  // Confirm that a non existing file is not found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, resource_metadata_->GetResourceEntryById(
      "non_existing", &entry));
}

TEST_F(ResourceMetadataTest, Iterate) {
  std::unique_ptr<ResourceMetadata::Iterator> it =
      resource_metadata_->GetIterator();
  ASSERT_TRUE(it);

  int file_count = 0, directory_count = 0;
  for (; !it->IsAtEnd(); it->Advance()) {
    if (!it->GetValue().file_info().is_directory())
      ++file_count;
    else
      ++directory_count;
  }

  EXPECT_EQ(7, file_count);
  EXPECT_EQ(9, directory_count);
}

TEST_F(ResourceMetadataTest, DuplicatedNames) {
  std::string root_local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root"), &root_local_id));

  ResourceEntry entry;

  // When multiple entries with the same title are added in a single directory,
  // their base_names are de-duped.
  // - drive/root/foo
  // - drive/root/foo (1)
  std::string dir_id_0;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntryWithResourceId(
          "foo", "foo0", root_local_id), &dir_id_0));
  std::string dir_id_1;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntryWithResourceId(
          "foo", "foo1", root_local_id), &dir_id_1));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir_id_0, &entry));
  EXPECT_EQ("foo", entry.base_name());
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir_id_1, &entry));
  EXPECT_EQ("foo (1)", entry.base_name());

  // - drive/root/foo/bar.txt
  // - drive/root/foo/bar (1).txt
  // - drive/root/foo/bar (2).txt
  // ...
  // - drive/root/foo/bar (99).txt
  std::vector<std::string> file_ids(100);
  for (size_t i = 0; i < file_ids.size(); ++i) {
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
        CreateFileEntryWithResourceId(
            "bar.txt", base::StringPrintf("bar%d", static_cast<int>(i)),
            dir_id_0), &file_ids[i]));
  }

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_ids[0], &entry));
  EXPECT_EQ("bar.txt", entry.base_name());
  for (size_t i = 1; i < file_ids.size(); ++i) {
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
        file_ids[i], &entry)) << i;
    EXPECT_EQ(base::StringPrintf("bar (%d).txt", static_cast<int>(i)),
              entry.base_name());
  }

  // Same name but different parent. No renaming.
  // - drive/root/foo (1)/bar.txt
  std::string file_id_3;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId(
          "bar.txt", "bar_different_parent", dir_id_1), &file_id_3));

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id_3, &entry));
  EXPECT_EQ("bar.txt", entry.base_name());

  // Checks that the entries can be looked up by the de-duped paths.
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/foo/bar (2).txt"), &entry));
  EXPECT_EQ("bar2", entry.resource_id());
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe("drive/root/foo (1)/bar.txt"), &entry));
  EXPECT_EQ("bar_different_parent", entry.resource_id());
}

TEST_F(ResourceMetadataTest, EncodedNames) {
  std::string root_local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetIdByPath(
      base::FilePath::FromUTF8Unsafe("drive/root"), &root_local_id));

  ResourceEntry entry;

  std::string dir_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateDirectoryEntry("\\(^o^)/", root_local_id), &dir_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      dir_id, &entry));
  EXPECT_EQ("\\(^o^)_", entry.base_name());

  std::string file_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
      CreateFileEntryWithResourceId("Slash /.txt", "myfile", dir_id),
      &file_id));
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryById(
      file_id, &entry));
  EXPECT_EQ("Slash _.txt", entry.base_name());

  ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->GetResourceEntryByPath(
      base::FilePath::FromUTF8Unsafe(
          "drive/root/\\(^o^)_/Slash _.txt"),
      &entry));
  EXPECT_EQ("myfile", entry.resource_id());
}

TEST_F(ResourceMetadataTest, Reset) {
  // The grand root has "root" which is not empty.
  std::vector<ResourceEntry> entries;
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/root"), &entries));
  ASSERT_FALSE(entries.empty());

  // Reset.
  EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->Reset());

  // change stamp should be reset.
  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetStartPageToken(&start_page_token));
  EXPECT_TRUE(start_page_token.empty());

  // root should continue to exist.
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->GetResourceEntryByPath(
                base::FilePath::FromUTF8Unsafe("drive"), &entry));
  EXPECT_EQ("drive", entry.base_name());
  ASSERT_TRUE(entry.file_info().is_directory());
  EXPECT_EQ(util::kDriveGrandRootLocalId, entry.local_id());

  // There are "other", "trash" and "root", "team_drives" and "Computers"
  // under "drive".
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive"), &entries));
  EXPECT_EQ(5U, entries.size());

  // The "other" directory should be empty.
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/other"), &entries));
  EXPECT_TRUE(entries.empty());

  // The "trash" directory should be empty.
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/trash"), &entries));
  EXPECT_TRUE(entries.empty());

  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/team_drives"), &entries));
  EXPECT_TRUE(entries.empty());

  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata_->ReadDirectoryByPath(
                base::FilePath::FromUTF8Unsafe("drive/Computers"), &entries));
  EXPECT_TRUE(entries.empty());
}

}  // namespace internal
}  // namespace drive
