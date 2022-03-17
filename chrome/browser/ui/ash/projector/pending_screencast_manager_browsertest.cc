// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

constexpr char kTestScreencastPath[] = "/root/test_screencast";
constexpr char kTestScreencastName[] = "test_screencast";
constexpr char kTestMediaFile[] = "test_screencast.webm";
constexpr char kTestMetadataFile[] = "test_screencast.projector";
// constexpr char kTestDataToWrite[] = "Data size of 16.";
// The test media file is 0.7 mb.
constexpr int64_t kTestMediaFileBytes = 700 * 1024;
// The test metadata file is 0.1 mb.
constexpr int64_t kTestMetadataFileBytes = 100 * 1024;

}  // namespace

// TODO(b/211000693) Replace all RunAllTasksUntilIdle with a waiting condition.
class PendingScreencastMangerBrowserTest : public InProcessBrowserTest {
 public:
  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &PendingScreencastMangerBrowserTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    pending_screencast_manager_ = std::make_unique<
        PendingSreencastManager>(base::BindRepeating(
        &PendingScreencastMangerBrowserTest::PendingScreencastChangeCallback,
        base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    pending_screencast_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore non-regular profile.
    if (!ProfileHelper::IsRegularProfile(profile))
      return nullptr;

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");

    fake_drivefs_helper_ =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  void CreateFileInDriveFsFolder(const std::string& file_path,
                                 int64_t total_bytes) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath relative_file_path(file_path);
    base::FilePath folder_path =
        GetDriveFsAbsolutePath(relative_file_path.DirName().value());

    // base::CreateDirectory returns 'true' on successful creation, or if the
    // directory already exists.
    EXPECT_TRUE(base::CreateDirectory(folder_path));

    base::File file(folder_path.Append(relative_file_path.BaseName()),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    // Create a buffer whose size is `total_bytes`.
    std::string buffer(total_bytes, 'a');
    EXPECT_EQ(total_bytes,
              file.Write(/*offset=*/0, buffer.data(), /*size=*/total_bytes));
    EXPECT_TRUE(file.IsValid());
    file.Close();
  }

  // Create a file for given `file_path`, which is a relative file path of
  // drivefs. Write `total_bytes` to this file. Create a drivefs syncing event
  // for this file with `transferred_bytes` transferred and add this event to
  // `syncing_status`.
  void CreateFileAndTransferItemEvent(
      const std::string& file_path,
      int64_t total_bytes,
      int64_t transferred_bytes,
      drivefs::mojom::SyncingStatus& syncing_status) {
    CreateFileInDriveFsFolder(file_path, total_bytes);
    AddTransferItemEvent(syncing_status, file_path, total_bytes,
                         transferred_bytes);
  }

  base::Time GetFileCreatedTime(const std::string& relative_file_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::File::Info info;
    return base::GetFileInfo(GetDriveFsAbsolutePath(relative_file_path), &info)
               ? info.creation_time
               : base::Time();
  }

  base::FilePath GetDriveFsAbsolutePath(const std::string& relative_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    drive::DriveIntegrationService* service =
        drive::DriveIntegrationServiceFactory::FindForProfile(
            browser()->profile());
    EXPECT_TRUE(service->IsMounted());
    EXPECT_TRUE(base::PathExists(service->GetMountPointPath()));

    base::FilePath root("/");
    base::FilePath absolute_path(service->GetMountPointPath());
    root.AppendRelativePath(base::FilePath(relative_path), &absolute_path);
    return absolute_path;
  }

  void AddTransferItemEvent(drivefs::mojom::SyncingStatus& syncing_status,
                            const std::string& path,
                            int64_t total_bytes,
                            int64_t transferred_bytes) {
    syncing_status.item_events.emplace_back(
        base::in_place, /*stable_id=*/1, /*group_id=*/1, path,
        total_bytes == transferred_bytes
            ? drivefs::mojom::ItemEvent::State::kCompleted
            : drivefs::mojom::ItemEvent::State::kInProgress,
        /*bytes_transferred=*/transferred_bytes,
        /*bytes_to_transfer=*/total_bytes,
        drivefs::mojom::ItemEventReason::kTransfer);
  }

  MOCK_METHOD1(PendingScreencastChangeCallback,
               void(const PendingScreencastSet&));

  PendingSreencastManager* pending_screencast_manager() {
    return pending_screencast_manager_.get();
  }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  std::unique_ptr<PendingSreencastManager> pending_screencast_manager_;
};

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest, ValidScreencast) {
  const std::string media_file =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  const std::string metadata_file =
      base::StrCat({kTestScreencastPath, "/", kTestMetadataFile});
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create a valid pending screencast.
    CreateFileAndTransferItemEvent(media_file,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    CreateFileAndTransferItemEvent(metadata_file,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
  }

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();

  const PendingScreencastSet pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(pending_screencasts.size(), 1u);
  ash::PendingScreencast ps = *(pending_screencasts.begin());
  EXPECT_EQ(ps.container_dir, base::FilePath(kTestScreencastPath));
  EXPECT_EQ(ps.name, kTestScreencastName);
  EXPECT_EQ(ps.created_time, GetFileCreatedTime(media_file));

  // Tests PendingScreencastChangeCallback won't be invoked if pending
  // screencast status doesn't change.
  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(0);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();

  // Tests PendingScreencastChangeCallback will be invoked if pending
  // screencast status changes.
  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  syncing_status.item_events.clear();
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest, InvalidScreencasts) {
  const std::string media_only_path = "/root/media_only/example.webm";
  const std::string metadata_only_path =
      "/root/metadata_only/example.projector";
  const std::string avi = "/root/non_screencast_files/example.avi";
  const std::string mov = "/root/non_screencast_files/example.mov";
  const std::string mp4 = "/root/non_screencast_files/example.mp4";
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create an invalid screencast that only has webm medida file.
    CreateFileAndTransferItemEvent(media_only_path,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);

    // Create an invalid screencast that only has metadata file.
    CreateFileAndTransferItemEvent(metadata_only_path,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);

    // Create an invalid screencast that does not have webm media and metadata
    // files but have other media files.
    CreateFileAndTransferItemEvent(avi, /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    CreateFileAndTransferItemEvent(mov, /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    CreateFileAndTransferItemEvent(mp4, /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
  }

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(0);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       IgnoreCompletedEvent) {
  const std::string media_file =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  const std::string metadata_file =
      base::StrCat({kTestScreencastPath, "/", kTestMetadataFile});
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create a valid uploaded screencast.
    CreateFileAndTransferItemEvent(media_file,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   kTestMediaFileBytes, syncing_status);
    CreateFileAndTransferItemEvent(metadata_file,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   kTestMetadataFileBytes, syncing_status);
  }

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(0);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       MultipleValidAndInvalidScreencasts) {
  drivefs::mojom::SyncingStatus syncing_status;
  size_t num_of_screencasts = 10;
  {
    // Create multiple valid pending screencasts.
    for (size_t i = 0; i < num_of_screencasts; ++i) {
      const std::string test_screencast_path =
          base::StrCat({kTestScreencastPath, base::NumberToString(i)});
      const std::string media =
          base::StrCat({test_screencast_path, "/", kTestMediaFile});
      const std::string metadata =
          base::StrCat({test_screencast_path, "/", kTestMetadataFile});
      CreateFileAndTransferItemEvent(media, /*total_bytes=*/kTestMediaFileBytes,
                                     /*transferred_bytes=*/0, syncing_status);
      CreateFileAndTransferItemEvent(metadata,
                                     /*total_bytes=*/kTestMetadataFileBytes,
                                     /*transferred_bytes=*/0, syncing_status);
    }

    // Tests with a invalid screencast does not have metadata file.
    const std::string no_metadata_screencast = "/root/no_metadata/example.webm";
    CreateFileAndTransferItemEvent(no_metadata_screencast,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    // Tests with a invalid screencast does not have media file.
    const std::string no_media_screencast = "/root/no_media/example.projector";
    CreateFileAndTransferItemEvent(no_media_screencast,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);

    // Tests with a non-screencast file.
    const std::string non_screencast = "/root/non_screencast/example.txt";
    CreateFileAndTransferItemEvent(non_screencast, /*total_bytes=*/100,
                                   /*transferred_bytes=*/0, syncing_status);
  }

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);

  content::RunAllTasksUntilIdle();
  const PendingScreencastSet pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  int64_t total_size = kTestMediaFileBytes + kTestMetadataFileBytes;

  // Only valid screencasts could be processed.
  EXPECT_EQ(pending_screencasts.size(), num_of_screencasts);
  for (size_t i = 0; i < num_of_screencasts; ++i) {
    const std::string container_dir =
        base::StrCat({kTestScreencastPath, base::NumberToString(i)});
    const std::string name =
        base::StrCat({kTestScreencastName, base::NumberToString(i)});
    ash::PendingScreencast ps{base::FilePath(container_dir), name, total_size,
                              0};
    EXPECT_TRUE(pending_screencasts.find(ps) != pending_screencasts.end());
  }
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest, UploadProgress) {
  const std::string media_file_path =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  const std::string metadata_file_path =
      base::StrCat({kTestScreencastPath, "/", kTestMetadataFile});
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create a valid pending screencast.
    CreateFileAndTransferItemEvent(media_file_path,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    CreateFileAndTransferItemEvent(metadata_file_path,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
  }

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);

  content::RunAllTasksUntilIdle();

  const PendingScreencastSet pending_screencasts_1 =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(pending_screencasts_1.size(), 1u);
  ash::PendingScreencast ps = *(pending_screencasts_1.begin());
  const int total_size = kTestMediaFileBytes + kTestMetadataFileBytes;
  EXPECT_EQ(total_size, ps.total_size_in_bytes);
  EXPECT_EQ(0, ps.bytes_transferred);

  // Tests the metadata file finished transferred.
  // PendingScreencastChangeCallback won't be invoked if the difference is less
  // than kPendingScreencastDiffThresholdInBytes.
  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(0);
  syncing_status.item_events.clear();
  int64_t media_transferred_1_bytes = 1;
  int64_t metadata_transferred_bytes = kTestMetadataFileBytes;
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/media_transferred_1_bytes);
  // Create a completed transferred event for metadata.
  AddTransferItemEvent(syncing_status, metadata_file_path,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/metadata_transferred_bytes);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();
  const PendingScreencastSet pending_screencasts_2 =
      pending_screencast_manager()->GetPendingScreencasts();
  ps = *(pending_screencasts_2.begin());
  // The screencast status unchanged.
  EXPECT_EQ(total_size, ps.total_size_in_bytes);
  EXPECT_EQ(0, ps.bytes_transferred);

  // Tests PendingScreencastChangeCallback will be invoked if the difference of
  // transferred bytes is greater than kPendingScreencastDiffThresholdInBytes.
  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  syncing_status.item_events.clear();
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/kTestMediaFileBytes - 1);
  // Create a completed transferred event for metadata.
  AddTransferItemEvent(syncing_status, metadata_file_path,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/metadata_transferred_bytes);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();
  const PendingScreencastSet pending_screencasts_3 =
      pending_screencast_manager()->GetPendingScreencasts();
  ps = *(pending_screencasts_3.begin());
  // The screencast status changed.
  EXPECT_EQ(total_size, ps.total_size_in_bytes);

  // TODO(b/209854146) After fix b/209854146, the `ps.bytes_transferred` is
  // `total_size -1`.
  EXPECT_EQ(kTestMediaFileBytes - 1, ps.bytes_transferred);

  // Tests PendingScreencastChangeCallback will be invoked when all files
  // finished transferred.
  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  syncing_status.item_events.clear();
  // Create completed transferred events for both files.
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/kTestMediaFileBytes);
  AddTransferItemEvent(syncing_status, metadata_file_path,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/kTestMetadataFileBytes);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
}

// Test the comparison of pending screencast in a std::set.
IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       PendingScreencastSet) {
  // The `name` and `total_size_in_bytes` of screencast will not be compare in a
  // set.
  const base::FilePath container_dir_a = base::FilePath("/root/a");
  const std::string screencast_a_name = "a";
  const int64_t screencast_a_total_bytes = 2 * 1024 * 1024;
  ash::PendingScreencast screencast_a_1_byte_transferred{
      container_dir_a, screencast_a_name, screencast_a_total_bytes,
      /*bytes_transferred=*/1};
  ash::PendingScreencast screencast_a_1kb_transferred{
      container_dir_a, screencast_a_name, screencast_a_total_bytes,
      /*bytes_transferred=*/1024};
  ash::PendingScreencast screencast_a_700kb_transferred{
      container_dir_a, screencast_a_name, screencast_a_total_bytes,
      /*bytes_transferred=*/700 * 1024};

  const base::FilePath container_dir_b = base::FilePath("/root/b");
  const std::string screencast_b_name = "b";
  const int64_t screencast_b_total_bytes = 2 * 1024 * 1024;
  ash::PendingScreencast screencast_b_1_byte_transferred{
      container_dir_b, screencast_b_name, screencast_b_total_bytes,
      /*bytes_transferred=*/1};
  ash::PendingScreencast screencast_b_1kb_transferred{
      container_dir_b, screencast_b_name, screencast_b_total_bytes,
      /*bytes_transferred=*/1024};
  ash::PendingScreencast screencast_b_700kb_transferred{
      container_dir_b, screencast_b_name, screencast_b_total_bytes,
      /*bytes_transferred=*/700 * 1024};

  PendingScreencastSet set1{screencast_a_1_byte_transferred,
                            screencast_b_1_byte_transferred};
  PendingScreencastSet set2{screencast_a_1_byte_transferred,
                            screencast_b_1_byte_transferred};
  PendingScreencastSet set3{screencast_a_1kb_transferred,
                            screencast_b_1_byte_transferred};
  PendingScreencastSet set4{screencast_a_700kb_transferred,
                            screencast_b_1_byte_transferred};
  PendingScreencastSet set5{screencast_a_1_byte_transferred,
                            screencast_a_700kb_transferred};
  PendingScreencastSet set6{screencast_a_700kb_transferred,
                            screencast_a_1_byte_transferred};
  PendingScreencastSet set7{screencast_a_1_byte_transferred,
                            screencast_a_1kb_transferred};

  EXPECT_EQ(set1, set2);
  EXPECT_EQ(set1, set3);
  EXPECT_NE(set1, set4);
  EXPECT_NE(set1, set5);
  EXPECT_EQ(set5, set6);
  EXPECT_EQ(2u, set5.size());
  EXPECT_EQ(2u, set7.size());
}

// Test a screencast failed to upload will remain a "fail to upload" error state
// until it get successfully uploaded.
IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       DriveOutOfSpaceError) {
  const std::string media_file_path =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  const std::string metadata_file_path =
      base::StrCat({kTestScreencastPath, "/", kTestMetadataFile});
  drivefs::mojom::SyncingStatus syncing_status;
  // Create a valid pending screencast.
  CreateFileAndTransferItemEvent(media_file_path,
                                 /*total_bytes=*/kTestMediaFileBytes,
                                 /*transferred_bytes=*/0, syncing_status);
  CreateFileAndTransferItemEvent(metadata_file_path,
                                 /*total_bytes=*/kTestMetadataFileBytes,
                                 /*transferred_bytes=*/0, syncing_status);
  content::RunAllTasksUntilIdle();

  // Mock DriveFs sends an out of space error for media file.
  drivefs::mojom::DriveError error{
      drivefs::mojom::DriveError::Type::kCantUploadStorageFull,
      base::FilePath(media_file_path)};
  pending_screencast_manager()->OnError(error);

  // Even there's DriveError, DriveFs will keep trying to sync both metadata and
  // media file.
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();

  // Verify we have a fail status screencast.
  const PendingScreencastSet pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(1, pending_screencasts.size());
  ash::PendingScreencast ps = *(pending_screencasts.begin());
  EXPECT_TRUE(ps.upload_failed);

  // Mock both metadata and media file get uploaded.
  syncing_status.item_events.clear();
  // Create completed transferred events for both files.
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/kTestMediaFileBytes);
  AddTransferItemEvent(syncing_status, metadata_file_path,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/kTestMetadataFileBytes);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();

  // Expect the screencast get removed from pending screencasts set .
  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
}

class PendingScreencastMangerMultiProfileTest : public LoginManagerTest {
 public:
  PendingScreencastMangerMultiProfileTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    pending_screencast_manager_ =
        std::make_unique<PendingSreencastManager>(base::BindLambdaForTesting(
            [&](const PendingScreencastSet& set) { base::DoNothing(); }));
  }

  void TearDownOnMainThread() override {
    pending_screencast_manager_.reset();
    LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  std::unique_ptr<PendingSreencastManager> pending_screencast_manager_;
};

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerMultiProfileTest,
                       SwitchActiveUser) {
  LoginUser(account_id1_);

  // Verify DriveFsHost observation is observing user 1's DriveFsHost.
  Profile* profile1 = ProfileHelper::Get()->GetProfileByAccountId(account_id1_);
  drive::DriveIntegrationService* service_for_account1 =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile1);
  EXPECT_TRUE(pending_screencast_manager_->IsDriveFsObservationObservingSource(
      service_for_account1->GetDriveFsHost()));

  // Add user 2.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  // Verify DriveFsHost observation is observing user 2's DriveFsHost.
  Profile* profile2 = ProfileHelper::Get()->GetProfileByAccountId(account_id2_);
  drive::DriveIntegrationService* service_for_account2 =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile2);
  EXPECT_TRUE(pending_screencast_manager_->IsDriveFsObservationObservingSource(
      service_for_account2->GetDriveFsHost()));

  // Switch back to user1.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  // Verify DriveFsHost observation is observing user 1's DriveFsHost.
  EXPECT_TRUE(pending_screencast_manager_->IsDriveFsObservationObservingSource(
      service_for_account1->GetDriveFsHost()));
}

}  // namespace ash
