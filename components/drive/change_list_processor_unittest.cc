// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/change_list_processor.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/fake_free_disk_space_getter.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/drive.pb.h"
#include "components/drive/file_change.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

constexpr char kBaseStartPageToken[] = "123";
constexpr char kRootId[] = "fake_root";

enum FileOrDirectory {
  FILE,
  DIRECTORY,
};

struct EntryExpectation {
  std::string path;
  std::string id;
  std::string parent_id;
  FileOrDirectory type;
};

// Returns a basic change list which contains some files and directories.
std::vector<std::unique_ptr<ChangeList>> CreateBaseChangeList() {
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  // Add directories to the change list.
  ResourceEntry directory;
  directory.mutable_file_info()->set_is_directory(true);

  directory.set_title("Directory 1");
  directory.set_resource_id("1_folder_resource_id");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  directory.set_title("Sub Directory Folder");
  directory.set_resource_id("sub_dir_folder_resource_id");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "1_folder_resource_id");

  directory.set_title("Sub Sub Directory Folder");
  directory.set_resource_id("sub_sub_directory_folder_id");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "sub_dir_folder_resource_id");

  directory.set_title("Directory 2 excludeDir-test");
  directory.set_resource_id("sub_dir_folder_2_self_link");
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  // Add files to the change list.
  ResourceEntry file;

  file.set_title("File 1.txt");
  file.set_resource_id("2_file_resource_id");
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  file.set_title("SubDirectory File 1.txt");
  file.set_resource_id("subdirectory_file_1_id");
  Property* const property = file.mutable_new_properties()->Add();
  property->set_key("hello");
  property->set_value("world");
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "1_folder_resource_id");

  file.set_title("Orphan File 1.txt");
  file.set_resource_id("1_orphanfile_resource_id");
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back("");

  change_lists[0]->set_new_start_page_token(kBaseStartPageToken);
  return change_lists;
}

class ChangeListProcessorTest : public testing::Test {
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

    metadata_.reset(
        new internal::ResourceMetadata(metadata_storage_.get(), cache_.get(),
                                       base::ThreadTaskRunnerHandle::Get()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());
  }

  // Applies the |changes| to |metadata_| as a full resource list of
  // start page token |kBaseStartPageToken|.
  FileError ApplyFullResourceList(
      std::vector<std::unique_ptr<ChangeList>> changes) {
    ChangeListProcessor processor(util::kTeamDriveIdDefaultCorpus,
                                  util::GetDriveMyDriveRootPath(),
                                  metadata_.get(), nullptr);
    return processor.ApplyUserChangeList(kBaseStartPageToken, kRootId,
                                         std::move(changes),
                                         false /* is_delta_update */);
  }

  // Applies |changes| to |metadata_| as a delta update. The |changes| are
  // treated as user's changelists. Delta changelists should contain their
  // start page token in themselves. |changede_team_drives| returns any team
  // drives that were added or removed as part of the change list.
  FileError ApplyUserChangeList(
      std::vector<std::unique_ptr<ChangeList>> changes,
      FileChange* changed_files,
      FileChange* changed_team_drives) {
    ChangeListProcessor processor(util::kTeamDriveIdDefaultCorpus,
                                  util::GetDriveMyDriveRootPath(),
                                  metadata_.get(), nullptr);
    FileError error = processor.ApplyUserChangeList(kBaseStartPageToken,
                                                    kRootId, std::move(changes),
                                                    true /* is_delta_update */);
    *changed_files = processor.changed_files();
    *changed_team_drives = processor.changed_team_drives();
    return error;
  }

  // Gets the resource entry for the path from |metadata_| synchronously.
  // Returns null if the entry does not exist.
  std::unique_ptr<ResourceEntry> GetResourceEntry(const std::string& path) {
    std::unique_ptr<ResourceEntry> entry(new ResourceEntry);
    FileError error = metadata_->GetResourceEntryByPath(
        base::FilePath::FromUTF8Unsafe(path), entry.get());
    if (error != FILE_ERROR_OK)
      entry.reset();
    return entry;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  std::unique_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  std::unique_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  std::unique_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
};

}  // namespace

TEST_F(ChangeListProcessorTest, ApplyFullResourceList) {
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  const EntryExpectation kExpected[] = {
      // Root files
      {"drive/root", kRootId, "", DIRECTORY},
      {"drive/root/File 1.txt",
          "2_file_resource_id", kRootId, FILE},
      // Subdirectory files
      {"drive/root/Directory 1",
          "1_folder_resource_id", kRootId, DIRECTORY},
      {"drive/root/Directory 1/SubDirectory File 1.txt",
          "subdirectory_file_1_id", "1_folder_resource_id", FILE},
      {"drive/root/Directory 2 excludeDir-test",
          "sub_dir_folder_2_self_link", kRootId, DIRECTORY},
      // Deeper
      {"drive/root/Directory 1/Sub Directory Folder",
          "sub_dir_folder_resource_id",
          "1_folder_resource_id", DIRECTORY},
      {"drive/root/Directory 1/Sub Directory Folder/Sub Sub Directory Folder",
          "sub_sub_directory_folder_id",
          "sub_dir_folder_resource_id", DIRECTORY},
      // Orphan
      {"drive/other/Orphan File 1.txt", "1_orphanfile_resource_id",
           "", FILE},
  };

  for (size_t i = 0; i < arraysize(kExpected); ++i) {
    std::unique_ptr<ResourceEntry> entry = GetResourceEntry(kExpected[i].path);
    ASSERT_TRUE(entry) << "for path: " << kExpected[i].path;
    EXPECT_EQ(kExpected[i].id, entry->resource_id());

    ResourceEntry parent_entry;
    EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryById(
        entry->parent_local_id(), &parent_entry));
    EXPECT_EQ(kExpected[i].parent_id, parent_entry.resource_id());
    EXPECT_EQ(kExpected[i].type,
              entry->file_info().is_directory() ? DIRECTORY : FILE);
  }

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ(kBaseStartPageToken, start_page_token);
}

TEST_F(ChangeListProcessorTest, DeltaFileAddedInNewDirectory) {
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry new_folder;
  new_folder.set_resource_id("new_folder_resource_id");
  new_folder.set_title("New Directory");
  new_folder.mutable_file_info()->set_is_directory(true);
  change_lists[0]->mutable_entries()->push_back(new_folder);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  ResourceEntry new_file;
  new_file.set_resource_id("file_added_in_new_dir_id");
  new_file.set_title("File in new dir.txt");
  change_lists[0]->mutable_entries()->push_back(new_file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      new_folder.resource_id());

  change_lists[0]->set_new_start_page_token("16730");

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16730", start_page_token);
  EXPECT_TRUE(GetResourceEntry("drive/root/New Directory"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/New Directory/File in new dir.txt"));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/New Directory/File in new dir.txt")));
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/New Directory")));
}

TEST_F(ChangeListProcessorTest, DeltaDirMovedFromRootToDirectory) {
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry entry;
  entry.set_resource_id("1_folder_resource_id");
  entry.set_title("Directory 1");
  entry.mutable_file_info()->set_is_directory(true);
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "sub_dir_folder_2_self_link");

  change_lists[0]->set_new_start_page_token("16809");

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16809", start_page_token);
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1"));
  EXPECT_TRUE(GetResourceEntry(
      "drive/root/Directory 2 excludeDir-test/Directory 1"));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.CountDirectory(
      base::FilePath::FromUTF8Unsafe("drive/root")));
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
  EXPECT_TRUE(changed_files.CountDirectory(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 2 excludeDir-test")));
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 2 excludeDir-test/Directory 1")));
}

TEST_F(ChangeListProcessorTest, DeltaFileMovedFromDirectoryToRoot) {
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry entry;
  entry.set_resource_id("subdirectory_file_1_id");
  entry.set_title("SubDirectory File 1.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_new_start_page_token("16815");

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16815", start_page_token);
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  EXPECT_TRUE(GetResourceEntry("drive/root/SubDirectory File 1.txt"));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/SubDirectory File 1.txt")));
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 1/SubDirectory File 1.txt")));
}

TEST_F(ChangeListProcessorTest, DeltaFileRenamedInDirectory) {
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry entry;
  entry.set_resource_id("subdirectory_file_1_id");
  entry.set_title("New SubDirectory File 1.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "1_folder_resource_id");

  change_lists[0]->set_new_start_page_token("16767");

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(2U, changed_files.size());
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 1/SubDirectory File 1.txt")));
  EXPECT_TRUE(changed_files.count(base::FilePath::FromUTF8Unsafe(
      "drive/root/Directory 1/New SubDirectory File 1.txt")));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16767", start_page_token);
  EXPECT_FALSE(GetResourceEntry(
      "drive/root/Directory 1/SubDirectory File 1.txt"));
  std::unique_ptr<ResourceEntry> new_entry(
      GetResourceEntry("drive/root/Directory 1/New SubDirectory File 1.txt"));
  ASSERT_TRUE(new_entry);

  // Keep the to-be-synced properties.
  ASSERT_EQ(1, new_entry->mutable_new_properties()->size());
  const Property& new_property = new_entry->new_properties().Get(0);
  EXPECT_EQ("hello", new_property.key());
}

TEST_F(ChangeListProcessorTest, DeltaAddAndDeleteFileInRoot) {
  // Create ChangeList to add a file.
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry entry;
  entry.set_resource_id("added_in_root_id");
  entry.set_title("Added file.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_new_start_page_token("16683");

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16683", start_page_token);
  EXPECT_TRUE(GetResourceEntry("drive/root/Added file.txt"));
  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Added file.txt")));

  // Create ChangeList to delete the file.
  change_lists.push_back(std::make_unique<ChangeList>());

  entry.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_new_start_page_token("16687");

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16687", start_page_token);
  EXPECT_FALSE(GetResourceEntry("drive/root/Added file.txt"));
  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Added file.txt")));
}


TEST_F(ChangeListProcessorTest, DeltaAddAndDeleteFileFromExistingDirectory) {
  // Create ChangeList to add a file.
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry entry;
  entry.set_resource_id("added_in_root_id");
  entry.set_title("Added file.txt");
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "1_folder_resource_id");

  change_lists[0]->set_new_start_page_token("16730");

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));
  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16730", start_page_token);
  EXPECT_TRUE(GetResourceEntry("drive/root/Directory 1/Added file.txt"));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1/Added file.txt")));

  // Create ChangeList to delete the file.
  change_lists.push_back(std::make_unique<ChangeList>());

  entry.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(entry);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "1_folder_resource_id");

  change_lists[0]->set_new_start_page_token("16770");

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16770", start_page_token);
  EXPECT_FALSE(GetResourceEntry("drive/root/Directory 1/Added file.txt"));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(changed_files.count(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1/Added file.txt")));
}

TEST_F(ChangeListProcessorTest, DeltaAddFileToNewButDeletedDirectory) {
  // Create a change which contains the following updates:
  // 1) A new PDF file is added to a new directory
  // 2) but the new directory is marked "deleted" (i.e. moved to Trash)
  // Hence, the PDF file should be just ignored.
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry file;
  file.set_resource_id("file_added_in_deleted_id");
  file.set_title("new_pdf_file.pdf");
  file.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(file);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      "new_folder_resource_id");

  ResourceEntry directory;
  directory.set_resource_id("new_folder_resource_id");
  directory.set_title("New Directory");
  directory.mutable_file_info()->set_is_directory(true);
  directory.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(directory);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);

  change_lists[0]->set_new_start_page_token("16730");

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16730", start_page_token);
  EXPECT_FALSE(GetResourceEntry("drive/root/New Directory/new_pdf_file.pdf"));

  EXPECT_TRUE(changed_team_drives.empty());
  EXPECT_TRUE(changed_files.empty());
}

TEST_F(ChangeListProcessorTest, RefreshDirectory) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change list.
  std::unique_ptr<ChangeList> change_list(new ChangeList);

  // Add a new file to the change list.
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  change_list->mutable_entries()->push_back(new_file);
  change_list->mutable_parent_resource_ids()->push_back(kRootId);

  // Add "Directory 1" to the map with a new name.
  ResourceEntry dir1;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII("Directory 1"), &dir1));
  dir1.set_title(dir1.title() + " (renamed)");
  change_list->mutable_entries()->push_back(dir1);
  change_list->mutable_parent_resource_ids()->push_back(kRootId);

  // Update the directory with the map.
  ResourceEntry root;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root));
  const std::string kNewStartpageToken = "12345";
  ResourceEntryVector refreshed_entries;
  EXPECT_EQ(FILE_ERROR_OK,
            ChangeListProcessor::RefreshDirectory(
                metadata_.get(),
                DirectoryFetchInfo(root.local_id(), kRootId, kNewStartpageToken,
                                   util::GetDriveMyDriveRootPath(),
                                   util::GetDriveMyDriveRootPath()),
                std::move(change_list), &refreshed_entries));

  // "new_file" should be added.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(new_file.title()), &entry));

  // "Directory 1" should be renamed.
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(dir1.title()), &entry));
}

TEST_F(ChangeListProcessorTest, RefreshDirectory_WrongParentId) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change list and add a new file to it.
  std::unique_ptr<ChangeList> change_list(new ChangeList);
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  // This entry should not be added because the parent ID does not match.
  change_list->mutable_parent_resource_ids()->push_back(
      "some-random-resource-id");
  change_list->mutable_entries()->push_back(new_file);


  // Update the directory.
  ResourceEntry root;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root));
  const std::string kNewStartpageToken = "12345";
  ResourceEntryVector refreshed_entries;
  EXPECT_EQ(FILE_ERROR_OK,
            ChangeListProcessor::RefreshDirectory(
                metadata_.get(),
                DirectoryFetchInfo(root.local_id(), kRootId, kNewStartpageToken,
                                   util::GetDriveMyDriveRootPath(),
                                   util::GetDriveMyDriveRootPath()),
                std::move(change_list), &refreshed_entries));

  // "new_file" should not be added.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath().AppendASCII(new_file.title()), &entry));
}

TEST_F(ChangeListProcessorTest, SharedFilesWithNoParentInFeed) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change lists.
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  // Add a new file with non-existing parent resource id to the change lists.
  ResourceEntry new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  change_lists[0]->mutable_entries()->push_back(new_file);
  change_lists[0]->mutable_parent_resource_ids()->push_back("nonexisting");
  change_lists[0]->set_new_start_page_token("123");

  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  // "new_file" should be added under drive/other.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveGrandRootPath().AppendASCII("other/new_file"), &entry));
}

TEST_F(ChangeListProcessorTest, ModificationDate) {
  // Prepare metadata.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  // Create change lists with a new file.
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  const base::Time now = base::Time::Now();
  ResourceEntry new_file_remote;
  new_file_remote.set_title("new_file_remote");
  new_file_remote.set_resource_id("new_file_id");
  new_file_remote.set_modification_date(now.ToInternalValue());

  change_lists[0]->mutable_entries()->push_back(new_file_remote);
  change_lists[0]->mutable_parent_resource_ids()->push_back(kRootId);
  change_lists[0]->set_new_start_page_token("123");

  // Add the same file locally, but with a different name, a dirty metadata
  // state, and a newer modification date.
  ResourceEntry root;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &root));

  ResourceEntry new_file_local;
  new_file_local.set_resource_id(new_file_remote.resource_id());
  new_file_local.set_parent_local_id(root.local_id());
  new_file_local.set_title("new_file_local");
  new_file_local.set_metadata_edit_state(ResourceEntry::DIRTY);
  new_file_local.set_modification_date(
      (now + base::TimeDelta::FromSeconds(1)).ToInternalValue());
  std::string local_id;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->AddEntry(new_file_local, &local_id));

  // Apply the change.
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  // The change is rejected due to the old modification date.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetResourceEntryById(local_id, &entry));
  EXPECT_EQ(new_file_local.title(), entry.title());
}

TEST_F(ChangeListProcessorTest, AddNewTeamDrive) {
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry team_drive;
  team_drive.set_resource_id("team_drive_resource_id");
  team_drive.set_title("New Team Drive");
  team_drive.mutable_file_info()->set_is_directory(true);
  team_drive.mutable_file_info()->set_is_team_drive_root(true);
  team_drive.set_parent_local_id(util::kDriveTeamDrivesDirLocalId);
  change_lists[0]->mutable_entries()->push_back(team_drive);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      util::kDriveTeamDrivesDirLocalId);

  change_lists[0]->set_new_start_page_token("16730");

  // Apply the changelist and check the effect.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));

  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16730", start_page_token);

  constexpr char kExpectedPath[] = "drive/team_drives/New Team Drive";
  EXPECT_TRUE(GetResourceEntry(kExpectedPath));

  // A new team drive will be in both changed_files and changed_team_drives.
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(
      changed_files.count(base::FilePath::FromUTF8Unsafe(kExpectedPath)));

  EXPECT_EQ(1U, changed_team_drives.size());
  EXPECT_TRUE(
      changed_team_drives.count(base::FilePath::FromUTF8Unsafe(kExpectedPath)));
}

TEST_F(ChangeListProcessorTest, AddAndDeleteTeamDrive) {
  // Create ChangeList to add a file.
  std::vector<std::unique_ptr<ChangeList>> change_lists;
  change_lists.push_back(std::make_unique<ChangeList>());

  ResourceEntry team_drive;
  team_drive.set_resource_id("team_drive_resource_id");
  team_drive.set_title("New Team Drive");
  team_drive.mutable_file_info()->set_is_directory(true);
  team_drive.mutable_file_info()->set_is_team_drive_root(true);
  team_drive.set_parent_local_id(util::kDriveTeamDrivesDirLocalId);
  change_lists[0]->mutable_entries()->push_back(team_drive);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      util::kDriveTeamDrivesDirLocalId);

  change_lists[0]->set_new_start_page_token("16683");

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK, ApplyFullResourceList(CreateBaseChangeList()));
  FileChange changed_files;
  FileChange changed_team_drives;
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16683", start_page_token);
  constexpr char kExpectedPath[] = "drive/team_drives/New Team Drive";

  EXPECT_TRUE(GetResourceEntry(kExpectedPath));
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(
      changed_files.count(base::FilePath::FromUTF8Unsafe(kExpectedPath)));
  EXPECT_EQ(1U, changed_team_drives.size());
  EXPECT_TRUE(
      changed_team_drives.count(base::FilePath::FromUTF8Unsafe(kExpectedPath)));

  // Create ChangeList to delete the file.
  change_lists.push_back(std::make_unique<ChangeList>());

  team_drive.set_deleted(true);
  change_lists[0]->mutable_entries()->push_back(team_drive);
  change_lists[0]->mutable_parent_resource_ids()->push_back(
      util::kDriveTeamDrivesDirLocalId);

  change_lists[0]->set_new_start_page_token("16687");

  // Apply.
  EXPECT_EQ(FILE_ERROR_OK,
            ApplyUserChangeList(std::move(change_lists), &changed_files,
                                &changed_team_drives));
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("16687", start_page_token);
  EXPECT_FALSE(GetResourceEntry(kExpectedPath));
  EXPECT_EQ(1U, changed_files.size());
  EXPECT_TRUE(
      changed_files.count(base::FilePath::FromUTF8Unsafe(kExpectedPath)));
  EXPECT_EQ(1U, changed_team_drives.size());
  EXPECT_TRUE(
      changed_team_drives.count(base::FilePath::FromUTF8Unsafe(kExpectedPath)));
}

}  // namespace internal
}  // namespace drive
