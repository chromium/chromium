// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/file_system.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/drive/chromeos/drive_change_list_loader.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/fake_free_disk_space_getter.h"
#include "components/drive/chromeos/file_system_observer.h"
#include "components/drive/chromeos/sync_client.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_change.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

// Counts the number of invocation, and if it increased up to |expected_counter|
// quits the current message loop by calling |quit|.
void AsyncInitializationCallback(int* counter,
                                 int expected_counter,
                                 const base::Closure& quit,
                                 FileError error,
                                 std::unique_ptr<ResourceEntry> entry) {
  if (error != FILE_ERROR_OK || !entry) {
    // If we hit an error case, quit the message loop immediately.
    // Then the expectation in the test case can find it because the actual
    // value of |counter| is different from the expected one.
    quit.Run();
    return;
  }

  (*counter)++;
  if (*counter >= expected_counter)
    quit.Run();
}

bool CompareHashAndFilePath(const HashAndFilePath& a,
                            const HashAndFilePath& b) {
  const int result = a.hash.compare(b.hash);
  if (result < 0)
    return true;
  if (result > 0)
    return false;
  return a.path.AsUTF8Unsafe().compare(b.path.AsUTF8Unsafe()) < 0;
}

// This class is used to record directory changes and examine them later.
class MockDirectoryChangeObserver : public FileSystemObserver {
 public:
  MockDirectoryChangeObserver() = default;
  ~MockDirectoryChangeObserver() override = default;

  // FileSystemObserver overrides.
  void OnDirectoryChanged(const base::FilePath& directory_path) override {
    changed_directories_.push_back(directory_path);
  }

  void OnFileChanged(const FileChange& new_file_change) override {
    changed_files_.Apply(new_file_change);
  }

  void OnTeamDrivesUpdated(
      const std::set<std::string>& added_team_drive_ids,
      const std::set<std::string>& removed_team_drive_ids) override {
    added_team_drive_ids_ = added_team_drive_ids;
    removed_team_drive_ids_ = removed_team_drive_ids;
  }

  const std::vector<base::FilePath>& changed_directories() const {
    return changed_directories_;
  }

  const FileChange& changed_files() const { return changed_files_; }

  const std::set<std::string>& added_team_drive_ids() const {
    return added_team_drive_ids_;
  }

  const std::set<std::string>& removed_team_drive_ids() const {
    return removed_team_drive_ids_;
  }

 private:
  std::vector<base::FilePath> changed_directories_;
  FileChange changed_files_;
  std::set<std::string> added_team_drive_ids_;
  std::set<std::string> removed_team_drive_ids_;

  DISALLOW_COPY_AND_ASSIGN(MockDirectoryChangeObserver);
};

struct DestroyHelper {
  // FileSystemTest needs to be default constructible, so we provide a default
  // constructor here.
  DestroyHelper() = default;

  explicit DestroyHelper(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  template <typename T>
  void operator()(T* object) const {
    DCHECK(task_runner_);
    if (object) {
      object->Destroy();
      task_runner_->RunUntilIdle();
    }
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

}  // namespace

class FileSystemTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::TestMockTimeTaskRunner::Type::kBoundToThread);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_util::RegisterDrivePrefs(pref_service_->registry());

    logger_ = std::make_unique<EventLogger>();
    fake_drive_service_ = std::make_unique<FakeDriveService>();
    test_util::SetUpTestEntries(fake_drive_service_.get());

    fake_free_disk_space_getter_ = std::make_unique<FakeFreeDiskSpaceGetter>();

    scheduler_ = std::make_unique<JobScheduler>(
        pref_service_.get(), logger_.get(), fake_drive_service_.get(),
        task_runner_.get(), nullptr);

    mock_directory_observer_ = std::make_unique<MockDirectoryChangeObserver>();

    SetUpResourceMetadataAndFileSystem();
  }

  void SetUpResourceMetadataAndFileSystem() {
    const base::FilePath metadata_dir = temp_dir_.GetPath().AppendASCII("meta");
    ASSERT_TRUE(base::CreateDirectory(metadata_dir));
    metadata_storage_ =
        std::unique_ptr<internal::ResourceMetadataStorage, DestroyHelper>(
            new internal::ResourceMetadataStorage(metadata_dir,
                                                  task_runner_.get()),
            DestroyHelper(task_runner_.get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    const base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII("files");
    ASSERT_TRUE(base::CreateDirectory(cache_dir));
    cache_ = std::unique_ptr<internal::FileCache, DestroyHelper>(
        new internal::FileCache(metadata_storage_.get(), cache_dir,
                                task_runner_.get(),
                                fake_free_disk_space_getter_.get()),
        DestroyHelper(task_runner_.get()));
    ASSERT_TRUE(cache_->Initialize());

    resource_metadata_ =
        std::unique_ptr<internal::ResourceMetadata, DestroyHelper>(
            new internal::ResourceMetadata(metadata_storage_.get(),
                                           cache_.get(), task_runner_.get()),
            DestroyHelper(task_runner_.get()));
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->Initialize());

    const base::FilePath temp_file_dir = temp_dir_.GetPath().AppendASCII("tmp");
    ASSERT_TRUE(base::CreateDirectory(temp_file_dir));
    file_system_ = std::make_unique<FileSystem>(
        logger_.get(), cache_.get(), scheduler_.get(), resource_metadata_.get(),
        task_runner_.get(), temp_file_dir, task_runner_->GetMockClock());
    file_system_->AddObserver(mock_directory_observer_.get());

    // Disable delaying so that the sync starts immediately.
    file_system_->sync_client_for_testing()->set_delay_for_testing(
        base::TimeDelta::FromSeconds(0));

    file_system_->team_drive_operation_queue_for_testing()
        ->DisableQueueForTesting();
  }

  // Loads the full resource list via FakeDriveService.
  bool LoadFullResourceList() {
    FileError error = FILE_ERROR_FAILED;
    file_system_->change_list_loader_for_testing()->LoadIfNeeded(
        google_apis::test_util::CreateCopyResultCallback(&error));
    task_runner_->RunUntilIdle();
    return error == FILE_ERROR_OK;
  }

  // Gets resource entry by path synchronously.
  std::unique_ptr<ResourceEntry> GetResourceEntrySync(
      const base::FilePath& file_path) {
    FileError error = FILE_ERROR_FAILED;
    std::unique_ptr<ResourceEntry> entry;
    file_system_->GetResourceEntry(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    task_runner_->RunUntilIdle();

    return entry;
  }

  // Gets directory info by path synchronously.
  std::unique_ptr<ResourceEntryVector> ReadDirectorySync(
      const base::FilePath& file_path) {
    FileError error = FILE_ERROR_FAILED;
    std::unique_ptr<ResourceEntryVector> entries(new ResourceEntryVector);
    file_system_->ReadDirectory(
        file_path,
        base::Bind(&AccumulateReadDirectoryResult, entries.get()),
        google_apis::test_util::CreateCopyResultCallback(&error));
    task_runner_->RunUntilIdle();
    if (error != FILE_ERROR_OK)
      entries.reset();
    return entries;
  }

  // Used to implement ReadDirectorySync().
  static void AccumulateReadDirectoryResult(
      ResourceEntryVector* out_entries,
      std::unique_ptr<ResourceEntryVector> entries) {
    ASSERT_TRUE(entries);
    out_entries->insert(out_entries->end(), entries->begin(), entries->end());
  }

  // Returns true if an entry exists at |file_path|.
  bool EntryExists(const base::FilePath& file_path) {
    return GetResourceEntrySync(file_path) != nullptr;
  }

  // Flag for specifying the timestamp of the test filesystem cache.
  enum SetUpTestFileSystemParam {
    USE_OLD_TIMESTAMP,
    USE_SERVER_TIMESTAMP,
  };

  // Sets up a filesystem with directories: drive/root, drive/root/Dir1,
  // drive/root/Dir1/SubDir2 and files drive/root/File1, drive/root/Dir1/File2,
  // drive/root/Dir1/SubDir2/File3. If |use_up_to_date_timestamp| is true, sets
  // the start_page_token to that of FakeDriveService, indicating the cache is
  // holding the latest file system info.
  void SetUpTestFileSystem(SetUpTestFileSystemParam param) {
    // Destroy the existing resource metadata to close DB.
    resource_metadata_.reset();

    const base::FilePath metadata_dir = temp_dir_.GetPath().AppendASCII("meta");
    ASSERT_TRUE(base::CreateDirectory(metadata_dir));
    std::unique_ptr<internal::ResourceMetadataStorage, DestroyHelper>
        metadata_storage(new internal::ResourceMetadataStorage(
                             metadata_dir, task_runner_.get()),
                         DestroyHelper(task_runner_.get()));

    const base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII("files");
    std::unique_ptr<internal::FileCache, DestroyHelper> cache(
        new internal::FileCache(metadata_storage.get(), cache_dir,
                                task_runner_.get(),
                                fake_free_disk_space_getter_.get()),
        DestroyHelper(task_runner_.get()));

    std::unique_ptr<internal::ResourceMetadata, DestroyHelper>
        resource_metadata(
            new internal::ResourceMetadata(metadata_storage_.get(), cache.get(),
                                           task_runner_.get()),
            DestroyHelper(task_runner_.get()));

    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->Initialize());

    const std::string start_page_token =
        param == USE_SERVER_TIMESTAMP
            ? fake_drive_service_->start_page_token().start_page_token()
            : "2";
    ASSERT_EQ(FILE_ERROR_OK,
              resource_metadata->SetStartPageToken(start_page_token));

    // drive/root
    ResourceEntry root;
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->GetResourceEntryByPath(
        util::GetDriveMyDriveRootPath(), &root));
    root.set_resource_id(fake_drive_service_->GetRootResourceId());
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->RefreshEntry(root));

    std::string local_id;

    // drive/root/File1
    ResourceEntry file1;
    file1.set_title("File1");
    file1.set_resource_id("resource_id:File1");
    file1.set_parent_local_id(root.local_id());
    file1.mutable_file_specific_info()->set_md5("md5#1");
    file1.mutable_file_info()->set_is_directory(false);
    file1.mutable_file_info()->set_size(1048576);
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(file1, &local_id));

    // drive/root/Dir1
    ResourceEntry dir1;
    dir1.set_title("Dir1");
    dir1.set_resource_id("resource_id:Dir1");
    dir1.set_parent_local_id(root.local_id());
    dir1.mutable_file_info()->set_is_directory(true);
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(dir1, &local_id));
    const std::string dir1_local_id = local_id;

    // drive/root/Dir1/File2
    ResourceEntry file2;
    file2.set_title("File2");
    file2.set_resource_id("resource_id:File2");
    file2.set_parent_local_id(dir1_local_id);
    file2.mutable_file_specific_info()->set_md5("md5#2");
    file2.mutable_file_info()->set_is_directory(false);
    file2.mutable_file_info()->set_size(555);
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(file2, &local_id));

    // drive/root/Dir1/SubDir2
    ResourceEntry dir2;
    dir2.set_title("SubDir2");
    dir2.set_resource_id("resource_id:SubDir2");
    dir2.set_parent_local_id(dir1_local_id);
    dir2.mutable_file_info()->set_is_directory(true);
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(dir2, &local_id));
    const std::string dir2_local_id = local_id;

    // drive/root/Dir1/SubDir2/File3
    ResourceEntry file3;
    file3.set_title("File3");
    file3.set_resource_id("resource_id:File3");
    file3.set_parent_local_id(dir2_local_id);
    file3.mutable_file_specific_info()->set_md5("md5#2");
    file3.mutable_file_info()->set_is_directory(false);
    file3.mutable_file_info()->set_size(12345);
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(file3, &local_id));

    // drive/team_drive
    ResourceEntry team_drive_root;
    ASSERT_EQ(FILE_ERROR_OK,
              resource_metadata->GetResourceEntryByPath(
                  util::GetDriveTeamDrivesRootPath(), &team_drive_root));

    // drive/team_drive/team_drive_1
    ResourceEntry td_dir;
    td_dir.set_title("team_drive_1");
    td_dir.set_resource_id("td_id_1");
    td_dir.set_parent_local_id(team_drive_root.local_id());
    td_dir.mutable_file_info()->set_is_directory(true);
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata->AddEntry(td_dir, &local_id));

    // Recreate resource metadata.
    SetUpResourceMetadataAndFileSystem();
  }

  // Sets up two team drives, team_drive_a and team_drive_b and creates the
  // following:
  // - Directories:
  // -- team_drive_a/dir1
  // -- team_drive_a/dir1/nested_1
  // -- team_drive_b/dir2
  //
  // - Files:
  // -- team_drive_a/dir1/file1
  // -- team_drive_a/dir1/nested_1/file1
  // -- team_drive_b/dir2/file2
  bool SetupTeamDrives() {
    fake_drive_service_->AddTeamDrive("td_id_1", "team_drive_1", "");
    fake_drive_service_->AddTeamDrive("td_id_2", "team_drive_2", "");
    fake_drive_service_->AddTeamDrive("td_id_2_2", "team_drive_2", "");

    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;

    fake_drive_service_->AddNewFileWithResourceId(
        "td_1_dir_1_resource_id", util::kDriveFolderMimeType, std::string(),
        "td_id_1", "dir1",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    fake_drive_service_->AddNewFileWithResourceId(
        "td_1_dir_nested_1_resource_id", util::kDriveFolderMimeType,
        std::string(), "td_1_dir_1_resource_id", "nested_1",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    fake_drive_service_->AddNewFileWithResourceId(
        "td_2_dir_2_resource_id", util::kDriveFolderMimeType, std::string(),
        "td_id_2", "dir2",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    fake_drive_service_->AddNewFileWithResourceId(
        "dir1_file_1_resource_id", "audio/mpeg", "dir 1 file 1 content.",
        "td_1_dir_1_resource_id", "File 1.txt",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    fake_drive_service_->AddNewFileWithResourceId(
        "nested_1_file_1_resource_id", "audio/mpeg", "nested 1 file 1 content.",
        "td_1_dir_nested_1_resource_id", "Nested File 1.txt",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    fake_drive_service_->AddNewFileWithResourceId(
        "dir_2_file_1_resource_id", "audio/mpeg", "dir2 file1 content.",
        "td_2_dir_2_resource_id", "File 2.txt",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    fake_drive_service_->AddNewFileWithResourceId(
        "td_2_2_dir_1_resource_id", util::kDriveFolderMimeType, std::string(),
        "td_id_2_2", "dir1",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_CREATED)
      return false;

    return true;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;
  // We don't use TestingProfile::GetPrefs() in favor of having less
  // dependencies to Profile in general.
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::unique_ptr<EventLogger> logger_;
  std::unique_ptr<FakeDriveService> fake_drive_service_;
  std::unique_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  std::unique_ptr<JobScheduler> scheduler_;
  std::unique_ptr<MockDirectoryChangeObserver> mock_directory_observer_;

  std::unique_ptr<internal::ResourceMetadataStorage, DestroyHelper>
      metadata_storage_;
  std::unique_ptr<internal::FileCache, DestroyHelper> cache_;
  std::unique_ptr<internal::ResourceMetadata, DestroyHelper> resource_metadata_;
  std::unique_ptr<FileSystem> file_system_;
};

TEST_F(FileSystemTest, SearchByHashes) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));

  std::set<std::string> hashes;
  FileError error;
  std::vector<HashAndFilePath> results;

  hashes.insert("md5#1");
  file_system_->SearchByHashes(
      hashes,
      google_apis::test_util::CreateCopyResultCallback(&error, &results));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(FILE_PATH_LITERAL("drive/root/File1"), results[0].path.value());

  hashes.clear();
  hashes.insert("md5#2");
  file_system_->SearchByHashes(
      hashes,
      google_apis::test_util::CreateCopyResultCallback(&error, &results));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(2u, results.size());
  std::sort(results.begin(), results.end(), &CompareHashAndFilePath);
  EXPECT_EQ(FILE_PATH_LITERAL("drive/root/Dir1/File2"),
            results[0].path.value());
  EXPECT_EQ(FILE_PATH_LITERAL("drive/root/Dir1/SubDir2/File3"),
            results[1].path.value());

  hashes.clear();
  hashes.insert("md5#1");
  hashes.insert("md5#2");
  file_system_->SearchByHashes(
      hashes,
      google_apis::test_util::CreateCopyResultCallback(&error, &results));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(3u, results.size());
  std::sort(results.begin(), results.end(), &CompareHashAndFilePath);
  EXPECT_EQ(FILE_PATH_LITERAL("drive/root/File1"), results[0].path.value());
  EXPECT_EQ(FILE_PATH_LITERAL("drive/root/Dir1/File2"),
            results[1].path.value());
  EXPECT_EQ(FILE_PATH_LITERAL("drive/root/Dir1/SubDir2/File3"),
            results[2].path.value());
}

TEST_F(FileSystemTest, Copy) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL("drive/root/Copied.txt"));
  EXPECT_TRUE(GetResourceEntrySync(src_file_path));
  EXPECT_FALSE(GetResourceEntrySync(dest_file_path));

  FileError error = FILE_ERROR_FAILED;
  file_system_->Copy(src_file_path,
                     dest_file_path,
                     false,  // preserve_last_modified,
                     google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Entry is added on the server.
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(dest_file_path);
  ASSERT_TRUE(entry);

  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_EQ(entry->title(), server_entry->title());
  EXPECT_FALSE(server_entry->IsDirectory());
}

TEST_F(FileSystemTest, Move) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/Moved.txt"));
  EXPECT_TRUE(GetResourceEntrySync(src_file_path));
  EXPECT_FALSE(GetResourceEntrySync(dest_file_path));
  std::unique_ptr<ResourceEntry> parent =
      GetResourceEntrySync(dest_file_path.DirName());
  ASSERT_TRUE(parent);

  FileError error = FILE_ERROR_FAILED;
  file_system_->Move(src_file_path,
                     dest_file_path,
                     google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Entry is moved on the server.
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(dest_file_path);
  ASSERT_TRUE(entry);

  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_EQ(entry->title(), server_entry->title());

  ASSERT_FALSE(server_entry->parents().empty());
  EXPECT_EQ(parent->resource_id(), server_entry->parents()[0].file_id());
}

TEST_F(FileSystemTest, Remove) {
  base::FilePath file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);

  FileError error = FILE_ERROR_FAILED;
  file_system_->Remove(
      file_path,
      false,  // is_resursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Entry is removed on the server.
  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_TRUE(server_entry->labels().is_trashed());
}

TEST_F(FileSystemTest, CreateDirectory) {
  base::FilePath directory_path(FILE_PATH_LITERAL("drive/root/New Directory"));
  EXPECT_FALSE(GetResourceEntrySync(directory_path));

  FileError error = FILE_ERROR_FAILED;
  file_system_->CreateDirectory(
      directory_path,
      true,  // is_exclusive
      false,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Directory is created on the server.
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(directory_path);
  ASSERT_TRUE(entry);

  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_EQ(entry->title(), server_entry->title());
  EXPECT_TRUE(server_entry->IsDirectory());
}

TEST_F(FileSystemTest, CreateFile) {
  base::FilePath file_path(FILE_PATH_LITERAL("drive/root/New File.txt"));
  EXPECT_FALSE(GetResourceEntrySync(file_path));

  FileError error = FILE_ERROR_FAILED;
  file_system_->CreateFile(
      file_path,
      true,  // is_exclusive
      "text/plain",
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // File is created on the server.
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);

  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_EQ(entry->title(), server_entry->title());
  EXPECT_FALSE(server_entry->IsDirectory());
}

TEST_F(FileSystemTest, TouchFile) {
  base::FilePath file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);

  base::Time last_accessed =
      base::Time::FromInternalValue(entry->file_info().last_accessed()) +
      base::TimeDelta::FromSeconds(1);
  base::Time last_modified =
      base::Time::FromInternalValue(entry->file_info().last_modified()) +
      base::TimeDelta::FromSeconds(1);

  FileError error = FILE_ERROR_FAILED;
  file_system_->TouchFile(
      file_path,
      last_accessed,
      last_modified,
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // File is touched on the server.
  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_EQ(last_accessed, server_entry->last_viewed_by_me_date());
  EXPECT_EQ(last_modified, server_entry->modified_date());
  EXPECT_EQ(last_modified, server_entry->modified_by_me_date());
}

TEST_F(FileSystemTest, TruncateFile) {
  base::FilePath file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);

  const int64_t kLength = entry->file_info().size() + 100;

  FileError error = FILE_ERROR_FAILED;
  file_system_->TruncateFile(
      file_path,
      kLength,
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // File is touched on the server.
  google_apis::DriveApiErrorCode status = google_apis::DRIVE_OTHER_ERROR;
  std::unique_ptr<google_apis::FileResource> server_entry;
  fake_drive_service_->GetFileResource(
      entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status, &server_entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(server_entry);
  EXPECT_EQ(kLength, server_entry->file_size());
}

TEST_F(FileSystemTest, DuplicatedAsyncInitialization) {
  base::RunLoop loop;

  int counter = 0;

  file_system_->GetResourceEntry(
      base::FilePath(FILE_PATH_LITERAL("drive/root")),
      base::BindOnce(&AsyncInitializationCallback, &counter, 2,
                     loop.QuitClosure()));
  file_system_->GetResourceEntry(
      base::FilePath(FILE_PATH_LITERAL("drive/root")),
      base::BindOnce(&AsyncInitializationCallback, &counter, 2,
                     loop.QuitClosure()));
  loop.Run();  // Wait to get our result
  EXPECT_EQ(2, counter);

  EXPECT_EQ(1, fake_drive_service_->file_list_load_count());
}

TEST_F(FileSystemTest, GetGrandRootEntry) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(util::kDriveGrandRootLocalId, entry->local_id());
}

TEST_F(FileSystemTest, GetOtherDirEntry) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/other"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(util::kDriveOtherDirLocalId, entry->local_id());
}

TEST_F(FileSystemTest, GetMyDriveRoot) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(fake_drive_service_->GetRootResourceId(), entry->resource_id());

  // After "fast fetch" is done, full resource list is fetched.
  EXPECT_EQ(1, fake_drive_service_->file_list_load_count());
}

TEST_F(FileSystemTest, GetTeamDriveRoot) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/team_drives"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(util::kDriveTeamDrivesDirLocalId, entry->local_id());

  // After "fast fetch" is done, full resource list is fetched.
  EXPECT_EQ(1, fake_drive_service_->file_list_load_count());
}

TEST_F(FileSystemTest, GetExistingFile) {
  // Simulate the situation that full feed fetching takes very long time,
  // to test the recursive "fast fetch" feature is properly working.
  fake_drive_service_->set_never_return_all_file_list(true);

  const base::FilePath kFilePath(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ("subdirectory_file_1_id", entry->resource_id());

  EXPECT_EQ(1, fake_drive_service_->about_resource_load_count());
  EXPECT_EQ(2, fake_drive_service_->directory_load_count());
  EXPECT_EQ(1, fake_drive_service_->blocked_file_list_load_count());
}

TEST_F(FileSystemTest, GetExistingDocument) {
  const base::FilePath kFilePath(
      FILE_PATH_LITERAL("drive/root/Document 1 excludeDir-test.gdoc"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ("5_document_resource_id", entry->resource_id());
}

TEST_F(FileSystemTest, GetNonExistingFile) {
  const base::FilePath kFilePath(
      FILE_PATH_LITERAL("drive/root/nonexisting.file"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  EXPECT_FALSE(entry);
}

TEST_F(FileSystemTest, GetInSubSubdir) {
  const base::FilePath kFilePath(
      FILE_PATH_LITERAL("drive/root/Directory 1/Sub Directory Folder/"
                        "Sub Sub Directory Folder"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  ASSERT_EQ("sub_sub_directory_folder_id", entry->resource_id());
}

TEST_F(FileSystemTest, GetOrphanFile) {
  ASSERT_TRUE(LoadFullResourceList());

  // Entry without parents are placed under "drive/other".
  const base::FilePath kFilePath(
      FILE_PATH_LITERAL("drive/other/Orphan File 1.txt"));
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ("1_orphanfile_resource_id", entry->resource_id());
}

TEST_F(FileSystemTest, ReadDirectory_Root) {
  // ReadDirectory() should kick off the resource list loading.
  std::unique_ptr<ResourceEntryVector> entries(
      ReadDirectorySync(base::FilePath::FromUTF8Unsafe("drive")));
  // The root directory should be read correctly.
  ASSERT_TRUE(entries);
  ASSERT_EQ(5U, entries->size());

  // The found three directories should be /drive/root, /drive/other,
  // /drive/trash and /drive/team_drives.
  std::set<base::FilePath> found;
  for (size_t i = 0; i < entries->size(); ++i)
    found.insert(base::FilePath::FromUTF8Unsafe((*entries)[i].title()));
  EXPECT_EQ(5U, found.size());
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe(
                    util::kDriveMyDriveRootDirName)));
  EXPECT_EQ(1U, found.count(
                    base::FilePath::FromUTF8Unsafe(util::kDriveOtherDirName)));
  EXPECT_EQ(1U, found.count(
                    base::FilePath::FromUTF8Unsafe(util::kDriveTrashDirName)));
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe(
                    util::kDriveTeamDrivesDirName)));
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe(
                    util::kDriveComputersDirName)));
}

TEST_F(FileSystemTest, ReadDirectory_TeamDrivesRoot) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));
  ASSERT_TRUE(SetupTeamDrives());

  // The first load trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  // After the first load team drive data should be available for reading.
  std::unique_ptr<ResourceEntryVector> entries(
      ReadDirectorySync(base::FilePath::FromUTF8Unsafe("drive/team_drives")));
  // The root directory should be read correctly.
  ASSERT_TRUE(entries);
  ASSERT_EQ(3U, entries->size());

  std::multiset<base::FilePath> found;
  for (size_t i = 0; i < entries->size(); ++i) {
    found.insert(base::FilePath::FromUTF8Unsafe((*entries)[i].title()));
  }
  EXPECT_EQ(3U, found.size());
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe("team_drive_1")));
  EXPECT_EQ(2U, found.count(base::FilePath::FromUTF8Unsafe("team_drive_2")));
  EXPECT_EQ(1, fake_drive_service_->team_drive_list_load_count());
  EXPECT_EQ(3, fake_drive_service_->file_list_load_count());

  // We should be able to read from drive/team_drives/team_drive_1
  std::unique_ptr<ResourceEntryVector> team_drive_1_entries(ReadDirectorySync(
      base::FilePath::FromUTF8Unsafe("drive/team_drives/team_drive_1")));

  ASSERT_TRUE(team_drive_1_entries);
  ASSERT_EQ(1U, team_drive_1_entries->size());
  std::set<base::FilePath> team_drive_1_found;
  for (size_t i = 0; i < team_drive_1_entries->size(); ++i) {
    team_drive_1_found.insert(
        base::FilePath::FromUTF8Unsafe((*team_drive_1_entries)[i].title()));
  }
  EXPECT_EQ(1U, team_drive_1_found.size());
  EXPECT_EQ(1U,
            team_drive_1_found.count(base::FilePath::FromUTF8Unsafe("dir1")));
}

TEST_F(FileSystemTest, ReadDirectory_TeamDriveFolder) {
  ASSERT_TRUE(SetupTeamDrives());

  // The first load trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  // Add a new entry to drive/team_drives/team_drive_1
  // Create a file in the test directory.
  std::unique_ptr<google_apis::FileResource> entry;
  {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    fake_drive_service_->AddNewFile(
        "text/plain", "(dummy data)", "td_id_1", "TestFile",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
  }

  // Notify the update to the file system.
  file_system_->CheckForUpdates();
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<ResourceEntryVector> entries(ReadDirectorySync(
      base::FilePath::FromUTF8Unsafe("drive/team_drives/team_drive_1")));
  // The root directory should be read correctly.
  ASSERT_TRUE(entries);

  std::set<base::FilePath> found;
  for (size_t i = 0; i < entries->size(); ++i) {
    found.insert(base::FilePath::FromUTF8Unsafe((*entries)[i].title()));
  }
  EXPECT_EQ(2U, found.size());
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe("dir1")));
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe("TestFile")));
}

TEST_F(FileSystemTest, AddTeamDriveInChangeList) {
  ASSERT_TRUE(SetupTeamDrives());

  // The first load trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  // Add a new team drive to the fake file system.
  {
    fake_drive_service_->AddTeamDrive("td_id_3", "team_drive_3");
    base::RunLoop().RunUntilIdle();
  }

  // Notify the update to the file system, which will add the team drive
  file_system_->CheckForUpdates();
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<ResourceEntryVector> entries(
      ReadDirectorySync(base::FilePath::FromUTF8Unsafe("drive/team_drives/")));
  // The root directory should be read correctly.
  ASSERT_TRUE(entries);
  std::multiset<base::FilePath> found;
  for (size_t i = 0; i < entries->size(); ++i) {
    found.insert(base::FilePath::FromUTF8Unsafe((*entries)[i].title()));
  }
  EXPECT_EQ(4U, found.size());
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe("team_drive_3")));

  // Add a new entry to drive/team_drives/team_drive_3
  // Create a file in the test directory.
  std::unique_ptr<google_apis::FileResource> entry;
  {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    fake_drive_service_->AddNewFile(
        "text/plain", "(dummy data)", "td_id_3", "TestFile",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
  }

  // Notify the update to the file system.
  file_system_->CheckForUpdates();
  base::RunLoop().RunUntilIdle();

  entries = ReadDirectorySync(
      base::FilePath::FromUTF8Unsafe("drive/team_drives/team_drive_3"));
  // The root directory should be read correctly.
  ASSERT_TRUE(entries);

  found.clear();
  for (size_t i = 0; i < entries->size(); ++i) {
    found.insert(base::FilePath::FromUTF8Unsafe((*entries)[i].title()));
  }
  EXPECT_EQ(1U, found.size());
  EXPECT_EQ(1U, found.count(base::FilePath::FromUTF8Unsafe("TestFile")));
}

TEST_F(FileSystemTest, ReadDirectory_NonRootDirectory) {
  // ReadDirectory() should kick off the resource list loading.
  std::unique_ptr<ResourceEntryVector> entries(ReadDirectorySync(
      base::FilePath::FromUTF8Unsafe("drive/root/Directory 1")));
  // The non root directory should also be read correctly.
  // There was a bug (crbug.com/181487), which broke this behavior.
  // Make sure this is fixed.
  ASSERT_TRUE(entries);
  EXPECT_EQ(3U, entries->size());
}

TEST_F(FileSystemTest, LoadFileSystemFromUpToDateCache) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));

  // Kicks loading of cached file system and query for server update.
  EXPECT_TRUE(ReadDirectorySync(util::GetDriveMyDriveRootPath()));

  // SetUpTestFileSystem and FakeDriveService have the same
  // start_page_token (i.e. the local metadata is up to date), so no request for
  // new resource list (i.e., call to GetResourceList) should happen.
  EXPECT_EQ(0, fake_drive_service_->file_list_load_count());

  // Since the file system has verified that it holds the latest snapshot,
  // it should change its state to "loaded", which admits periodic refresh.
  // To test it, call CheckForUpdates and verify it does try to check updates.
  const int start_page_toke_load_count_before =
      fake_drive_service_->start_page_token_load_count();
  file_system_->CheckForUpdates();
  task_runner_->RunUntilIdle();
  EXPECT_LT(start_page_toke_load_count_before,
            fake_drive_service_->start_page_token_load_count());
}

TEST_F(FileSystemTest, LoadFileSystemFromCacheWhileOffline) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_OLD_TIMESTAMP));

  // Make GetResourceList fail for simulating offline situation. This will
  // leave the file system "loaded from cache, but not synced with server"
  // state.
  fake_drive_service_->set_offline(true);

  // Load the root.
  EXPECT_TRUE(ReadDirectorySync(util::GetDriveGrandRootPath()));
  // Loading of start page token should not happen as it's offline.
  EXPECT_EQ(0, fake_drive_service_->start_page_token_load_count());

  // Load "My Drive".
  EXPECT_TRUE(ReadDirectorySync(util::GetDriveMyDriveRootPath()));
  EXPECT_EQ(0, fake_drive_service_->start_page_token_load_count());

  // Tests that cached data can be loaded even if the server is not reachable.
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/root/File1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/root/Dir1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/root/Dir1/File2"))));
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/root/Dir1/SubDir2"))));
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/root/Dir1/SubDir2/File3"))));

  // Since the file system has at least succeeded to load cached snapshot,
  // the file system should be able to start periodic refresh.
  // To test it, call CheckForUpdates and verify it does try to check
  // updates, which will cause directory changes.
  fake_drive_service_->set_offline(false);

  file_system_->CheckForUpdates();

  task_runner_->RunUntilIdle();
  EXPECT_EQ(1, fake_drive_service_->start_page_token_load_count());
  EXPECT_EQ(1, fake_drive_service_->change_list_load_count());

  ASSERT_LE(0u, mock_directory_observer_->changed_directories().size());
  ASSERT_LE(1u, mock_directory_observer_->changed_files().size());
}

TEST_F(FileSystemTest, ReadDirectoryWhileRefreshing) {
  // Use old timestamp so the fast fetch will be performed.
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_OLD_TIMESTAMP));

  // The list of resources in "drive/root/Dir1" should be fetched.
  EXPECT_TRUE(ReadDirectorySync(base::FilePath(
      FILE_PATH_LITERAL("drive/root/Dir1"))));
  EXPECT_EQ(1, fake_drive_service_->directory_load_count());

  ASSERT_LE(1u, mock_directory_observer_->changed_directories().size());
}

TEST_F(FileSystemTest, GetResourceEntryNonExistentWhileRefreshing) {
  // Use old timestamp so the fast fetch will be performed.
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_OLD_TIMESTAMP));

  // If an entry is not found, parent directory's resource list is fetched.
  EXPECT_FALSE(GetResourceEntrySync(base::FilePath(
      FILE_PATH_LITERAL("drive/root/Dir1/NonExistentFile"))));
  EXPECT_EQ(1, fake_drive_service_->directory_load_count());

  ASSERT_LE(1u, mock_directory_observer_->changed_directories().size());
}

TEST_F(FileSystemTest, CreateDirectoryByImplicitLoad) {
  // Intentionally *not* calling LoadFullResourceList(), for testing that
  // CreateDirectory ensures the resource list is loaded before it runs.

  base::FilePath existing_directory(
      FILE_PATH_LITERAL("drive/root/Directory 1"));
  FileError error = FILE_ERROR_FAILED;
  file_system_->CreateDirectory(
      existing_directory,
      true,  // is_exclusive
      false,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();

  // It should fail because is_exclusive is set to true.
  EXPECT_EQ(FILE_ERROR_EXISTS, error);
}

TEST_F(FileSystemTest, CreateDirectoryRecursively) {
  // Intentionally *not* calling LoadFullResourceList(), for testing that
  // CreateDirectory ensures the resource list is loaded before it runs.

  base::FilePath new_directory(
      FILE_PATH_LITERAL("drive/root/Directory 1/a/b/c/d"));
  FileError error = FILE_ERROR_FAILED;
  file_system_->CreateDirectory(
      new_directory,
      true,  // is_exclusive
      true,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();

  EXPECT_EQ(FILE_ERROR_OK, error);

  std::unique_ptr<ResourceEntry> entry(GetResourceEntrySync(new_directory));
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->file_info().is_directory());
}

TEST_F(FileSystemTest, ReadDirectoryAfterUpdateWhileLoading) {
  // Simulate the situation that full feed fetching takes very long time,
  // to test the recursive "fast fetch" feature is properly working.
  fake_drive_service_->set_never_return_all_file_list(true);

  // On the fake server, create the test directory.
  std::unique_ptr<google_apis::FileResource> parent;
  {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    fake_drive_service_->AddNewDirectory(
        fake_drive_service_->GetRootResourceId(), "UpdateWhileLoadingTestDir",
        AddNewDirectoryOptions(),
        google_apis::test_util::CreateCopyResultCallback(&error, &parent));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
  }

  // Fetch the directory. Currently it is empty.
  std::unique_ptr<ResourceEntryVector> before =
      ReadDirectorySync(base::FilePath(
          FILE_PATH_LITERAL("drive/root/UpdateWhileLoadingTestDir")));
  ASSERT_TRUE(before);
  EXPECT_EQ(0u, before->size());

  // Create a file in the test directory.
  std::unique_ptr<google_apis::FileResource> entry;
  {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    fake_drive_service_->AddNewFile(
        "text/plain",
        "(dummy data)",
        parent->file_id(),
        "TestFile",
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
  }

  // Notify the update to the file system.
  file_system_->CheckForUpdates();

  // Fast forward the clock, so that a new read of the directory is started.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));

  // Read the directory once again. Although the full feed fetching is not yet
  // finished, the "fast fetch" of the directory works and the refreshed content
  // is returned.
  std::unique_ptr<ResourceEntryVector> after = ReadDirectorySync(base::FilePath(
      FILE_PATH_LITERAL("drive/root/UpdateWhileLoadingTestDir")));
  ASSERT_TRUE(after);
  EXPECT_EQ(1u, after->size());
}

TEST_F(FileSystemTest, PinAndUnpin) {
  ASSERT_TRUE(LoadFullResourceList());

  base::FilePath file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Get the file info.
  std::unique_ptr<ResourceEntry> entry(GetResourceEntrySync(file_path));
  ASSERT_TRUE(entry);

  // Pin the file.
  FileError error = FILE_ERROR_FAILED;
  file_system_->Pin(file_path,
                    google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->file_specific_info().cache_state().is_pinned());
  EXPECT_TRUE(entry->file_specific_info().cache_state().is_present());

  // Unpin the file.
  error = FILE_ERROR_FAILED;
  file_system_->Unpin(file_path,
                      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().cache_state().is_pinned());

  // Pinned file gets synced and it results in entry state changes.
  ASSERT_EQ(0u, mock_directory_observer_->changed_directories().size());
  ASSERT_EQ(1u, mock_directory_observer_->changed_files().size());
  EXPECT_EQ(1u,
            mock_directory_observer_->changed_files().CountDirectory(
                base::FilePath(FILE_PATH_LITERAL("drive/root"))));
}

TEST_F(FileSystemTest, PinAndUnpin_NotSynced) {
  ASSERT_TRUE(LoadFullResourceList());

  base::FilePath file_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Get the file info.
  std::unique_ptr<ResourceEntry> entry(GetResourceEntrySync(file_path));
  ASSERT_TRUE(entry);

  // Unpin the file just after pinning. File fetch should be cancelled.
  FileError error_pin = FILE_ERROR_FAILED;
  file_system_->Pin(
      file_path,
      google_apis::test_util::CreateCopyResultCallback(&error_pin));

  FileError error_unpin = FILE_ERROR_FAILED;
  file_system_->Unpin(
      file_path,
      google_apis::test_util::CreateCopyResultCallback(&error_unpin));

  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error_pin);
  EXPECT_EQ(FILE_ERROR_OK, error_unpin);

  // No cache file available because the sync was cancelled by Unpin().
  entry = GetResourceEntrySync(file_path);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().cache_state().is_present());
}

TEST_F(FileSystemTest, GetAvailableSpace) {
  FileError error = FILE_ERROR_OK;
  int64_t bytes_total;
  int64_t bytes_used;
  file_system_->GetAvailableSpace(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &bytes_total, &bytes_used));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(6789012345LL, bytes_used);
  EXPECT_EQ(9876543210LL, bytes_total);
}

TEST_F(FileSystemTest, MarkCacheFileAsMountedAndUnmounted) {
  ASSERT_TRUE(LoadFullResourceList());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Make the file cached.
  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  std::unique_ptr<ResourceEntry> entry;
  file_system_->GetFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Test for mounting.
  error = FILE_ERROR_FAILED;
  file_path.clear();
  file_system_->MarkCacheFileAsMounted(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  error = FILE_ERROR_FAILED;
  bool is_marked_as_mounted = false;
  file_system_->IsCacheFileMarkedAsMounted(
      file_in_root, google_apis::test_util::CreateCopyResultCallback(
                        &error, &is_marked_as_mounted));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(is_marked_as_mounted);

  // Cannot remove a cache entry while it's being mounted.
  EXPECT_EQ(FILE_ERROR_IN_USE, cache_->Remove(entry->local_id()));

  // Test for unmounting.
  error = FILE_ERROR_FAILED;
  file_system_->MarkCacheFileAsUnmounted(
      file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  error = FILE_ERROR_FAILED;
  is_marked_as_mounted = true;
  file_system_->IsCacheFileMarkedAsMounted(
      file_in_root, google_apis::test_util::CreateCopyResultCallback(
                        &error, &is_marked_as_mounted));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_FALSE(is_marked_as_mounted);

  // Now able to remove the cache entry.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Remove(entry->local_id()));
}

TEST_F(FileSystemTest, FreeDiskSpaceIfNeededFor) {
  ASSERT_TRUE(LoadFullResourceList());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Make the file cached.
  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  std::unique_ptr<ResourceEntry> entry;
  file_system_->GetFile(file_in_root,
                        google_apis::test_util::CreateCopyResultCallback(
                            &error, &file_path, &entry));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->file_specific_info().cache_state().is_present());

  bool available;
  file_system_->FreeDiskSpaceIfNeededFor(
      512LL << 40,
      google_apis::test_util::CreateCopyResultCallback(&available));
  task_runner_->RunUntilIdle();
  ASSERT_FALSE(available);

  entry = GetResourceEntrySync(file_in_root);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().cache_state().is_present());
}

TEST_F(FileSystemTest, DebugMetadata) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));
  ASSERT_TRUE(SetupTeamDrives());

  // The first load will trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  base::Time now = base::Time::Now();

  file_system_->CheckForUpdates();
  base::RunLoop().RunUntilIdle();

  FileSystemMetadata default_corpus_metadata;
  std::map<std::string, FileSystemMetadata> team_drive_metadata;

  file_system_->GetMetadata(google_apis::test_util::CreateCopyResultCallback(
      &default_corpus_metadata, &team_drive_metadata));
  base::RunLoop().RunUntilIdle();

  EXPECT_LE(now, default_corpus_metadata.last_update_check_time);
  EXPECT_FALSE(default_corpus_metadata.refreshing);
  EXPECT_EQ(FILE_ERROR_OK, default_corpus_metadata.last_update_check_error);
  EXPECT_EQ("654340", default_corpus_metadata.start_page_token);

  EXPECT_EQ(3UL, team_drive_metadata.size());
  EXPECT_FALSE(team_drive_metadata["td_id_1"].refreshing);
  EXPECT_EQ(util::GetDriveTeamDrivesRootPath().Append("team_drive_1").value(),
            team_drive_metadata["td_id_1"].path);
  EXPECT_LE(now, team_drive_metadata["td_id_1"].last_update_check_time);
  EXPECT_EQ("654345", team_drive_metadata["td_id_1"].start_page_token);
  EXPECT_EQ(FILE_ERROR_OK,
            team_drive_metadata["td_id_1"].last_update_check_error);

  EXPECT_FALSE(team_drive_metadata["td_id_2"].refreshing);
  EXPECT_EQ(util::GetDriveTeamDrivesRootPath().Append("team_drive_2").value(),
            team_drive_metadata["td_id_2"].path);
  EXPECT_LE(now, team_drive_metadata["td_id_2"].last_update_check_time);
  EXPECT_EQ("654346", team_drive_metadata["td_id_2"].start_page_token);
  EXPECT_EQ(FILE_ERROR_OK,
            team_drive_metadata["td_id_2"].last_update_check_error);

  EXPECT_FALSE(team_drive_metadata["td_id_2_2"].refreshing);
  EXPECT_EQ(util::GetDriveTeamDrivesRootPath().Append("team_drive_2").value(),
            team_drive_metadata["td_id_2_2"].path);
  EXPECT_LE(now, team_drive_metadata["td_id_2_2"].last_update_check_time);
  EXPECT_EQ("654347", team_drive_metadata["td_id_2_2"].start_page_token);
  EXPECT_EQ(FILE_ERROR_OK,
            team_drive_metadata["td_id_2_2"].last_update_check_error);
}

TEST_F(FileSystemTest, TeamDrivesChangesObserved) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));
  ASSERT_TRUE(SetupTeamDrives());

  // The first load will trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  // This is the initial set of team drives.
  EXPECT_EQ(3UL, mock_directory_observer_->added_team_drive_ids().size());
  EXPECT_TRUE(mock_directory_observer_->removed_team_drive_ids().empty());

  fake_drive_service_->AddTeamDrive("td_id_3", "team_drive_3");
  // TODO(slangley): Add support for removing a team drive in fake file service.
  base::RunLoop().RunUntilIdle();

  // Notify the update to the file system, which will add the team drive
  file_system_->CheckForUpdates();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1UL, mock_directory_observer_->added_team_drive_ids().size());
  EXPECT_EQ(1UL,
            mock_directory_observer_->added_team_drive_ids().count("td_id_3"));
  EXPECT_TRUE(mock_directory_observer_->removed_team_drive_ids().empty());
}

TEST_F(FileSystemTest, CheckUpdatesWithIds) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));
  ASSERT_TRUE(SetupTeamDrives());

  // The first load will trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  // Check a non existent team drive id, no other sources should be updated.
  file_system_->CheckForUpdates({"non_existant_team_drive_id"});
  base::RunLoop().RunUntilIdle();

  FileSystemMetadata default_corpus_metadata;
  std::map<std::string, FileSystemMetadata> team_drive_metadata;

  file_system_->GetMetadata(google_apis::test_util::CreateCopyResultCallback(
      &default_corpus_metadata, &team_drive_metadata));
  base::RunLoop().RunUntilIdle();

  const base::Time default_time;
  EXPECT_EQ(default_time, default_corpus_metadata.last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_1"].last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_2"].last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_2_2"].last_update_check_time);

  base::Time now = base::Time::Now();

  // Update just the default corpus.
  file_system_->CheckForUpdates({""});
  base::RunLoop().RunUntilIdle();

  file_system_->GetMetadata(google_apis::test_util::CreateCopyResultCallback(
      &default_corpus_metadata, &team_drive_metadata));
  base::RunLoop().RunUntilIdle();

  EXPECT_LE(now, default_corpus_metadata.last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_1"].last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_2"].last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_2_2"].last_update_check_time);

  // Update two team drives.
  now = base::Time::Now();
  file_system_->CheckForUpdates({"td_id_1", "td_id_2"});
  base::RunLoop().RunUntilIdle();

  file_system_->GetMetadata(google_apis::test_util::CreateCopyResultCallback(
      &default_corpus_metadata, &team_drive_metadata));
  base::RunLoop().RunUntilIdle();

  EXPECT_GE(now, default_corpus_metadata.last_update_check_time);
  EXPECT_LE(now, team_drive_metadata["td_id_1"].last_update_check_time);
  EXPECT_LE(now, team_drive_metadata["td_id_2"].last_update_check_time);
  EXPECT_EQ(default_time,
            team_drive_metadata["td_id_2_2"].last_update_check_time);

  // Update everything.
  now = base::Time::Now();
  file_system_->CheckForUpdates({"", "td_id_1", "td_id_2", "td_id_2_2"});
  base::RunLoop().RunUntilIdle();

  file_system_->GetMetadata(google_apis::test_util::CreateCopyResultCallback(
      &default_corpus_metadata, &team_drive_metadata));
  base::RunLoop().RunUntilIdle();

  EXPECT_LE(now, default_corpus_metadata.last_update_check_time);
  EXPECT_LE(now, team_drive_metadata["td_id_1"].last_update_check_time);
  EXPECT_LE(now, team_drive_metadata["td_id_2"].last_update_check_time);
  EXPECT_LE(now, team_drive_metadata["td_id_2_2"].last_update_check_time);
}

TEST_F(FileSystemTest, RemoveNonExistingTeamDrive) {
  ASSERT_NO_FATAL_FAILURE(SetUpTestFileSystem(USE_SERVER_TIMESTAMP));
  ASSERT_TRUE(SetupTeamDrives());

  // The first load will trigger the loading of team drives.
  ReadDirectorySync(base::FilePath::FromUTF8Unsafe("."));

  // Create a file change with a delete team drive, ensure file_system_ does not
  // crash.
  const base::FilePath path =
      util::GetDriveTeamDrivesRootPath().Append("team_drive_2");
  std::unique_ptr<ResourceEntry> entry = GetResourceEntrySync(path);
  ASSERT_TRUE(entry);

  drive::FileChange change;
  change.Update(path, *entry, FileChange::CHANGE_TYPE_DELETE);

  // First time should be removed.
  file_system_->OnTeamDrivesChanged(change);
  std::set<std::string> expected_changes = {"td_id_2"};
  EXPECT_EQ(expected_changes,
            mock_directory_observer_->removed_team_drive_ids());

  // Second time should be no changes, and no crash.
  file_system_->OnTeamDrivesChanged(change);
  expected_changes = {};
  EXPECT_EQ(expected_changes,
            mock_directory_observer_->removed_team_drive_ids());
}

}   // namespace drive
