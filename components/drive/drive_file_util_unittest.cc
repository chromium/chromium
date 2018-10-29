// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/drive/chromeos/drive_file_util.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/fake_free_disk_space_getter.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/drive.pb.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {
namespace {

// The start page token of the resource metadata used in DriveFileUtilTest.
constexpr char kTestStartPageToken[] = "123456";

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

void AddTeamDriveRootEntry(ResourceMetadata* resource_metadata,
                           const std::string& team_drive_id,
                           const std::string& team_drive_name) {
  std::string local_id;
  ASSERT_EQ(FILE_ERROR_OK, resource_metadata->GetIdByPath(
                               util::GetDriveTeamDrivesRootPath(), &local_id));

  std::string root_local_id = local_id;
  ASSERT_EQ(FILE_ERROR_OK,
            resource_metadata->AddEntry(
                CreateDirectoryEntryWithResourceId(
                    team_drive_name, team_drive_id, root_local_id),
                &local_id));
}

}  // namespace

// Tests for methods running on the blocking task runner.
class DriveFileUtilTest : public testing::Test {
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

    resource_metadata_.reset(
        new ResourceMetadata(metadata_storage_.get(), cache_.get(),
                             base::ThreadTaskRunnerHandle::Get()));

    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->Initialize());
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

TEST_F(DriveFileUtilTest, TeamDriveStartPageToken) {
  constexpr char kTeamDriveId[] = "team_drive_id_1";
  constexpr char kTeamDriveName[] = "My Team Drive";
  constexpr char kStartPageToken[] = "123456";

  AddTeamDriveRootEntry(resource_metadata_.get(), kTeamDriveId, kTeamDriveName);

  EXPECT_EQ(FILE_ERROR_OK, SetStartPageToken(resource_metadata_.get(),
                                             kTeamDriveId, kStartPageToken));
  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, GetStartPageToken(resource_metadata_.get(),
                                             kTeamDriveId, &start_page_token));
  EXPECT_EQ(kStartPageToken, start_page_token);
}

TEST_F(DriveFileUtilTest, TeamDriveStartPageToken_NoEntry) {
  constexpr char kTeamDriveId[] = "team_drive_id_1";
  constexpr char kStartPageToken[] = "123456";

  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            SetStartPageToken(resource_metadata_.get(), kTeamDriveId,
                              kStartPageToken));
  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetStartPageToken(resource_metadata_.get(), kTeamDriveId,
                              &start_page_token));
}

}  // namespace internal
}  // namespace drive
