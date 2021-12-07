// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
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

}  // namespace

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

  void CreateFile(const std::string& file_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    drive::DriveIntegrationService* service =
        drive::DriveIntegrationServiceFactory::FindForProfile(
            browser()->profile());
    EXPECT_TRUE(service->IsMounted());
    EXPECT_TRUE(base::PathExists(service->GetMountPointPath()));

    base::FilePath root("/");
    base::FilePath path(file_path);
    base::FilePath folder_path(service->GetMountPointPath());
    root.AppendRelativePath(path.DirName(), &folder_path);

    // base::CreateDirectory returns 'true' on successful creation, or if the
    // directory already exists.
    EXPECT_TRUE(base::CreateDirectory(folder_path));

    base::File file(folder_path.Append(path.BaseName()),
                    base::File::FLAG_CREATE | base::File::FLAG_READ);

    EXPECT_TRUE(file.IsValid());
    file.Close();
  }

  void AddTransferItemEvent(drivefs::mojom::SyncingStatus& syncing_status,
                            bool completed,
                            const std::string& path) {
    syncing_status.item_events.emplace_back(
        base::in_place, /*stable_id*/ 1, /*group_id*/ 1, path,
        completed ? drivefs::mojom::ItemEvent::State::kCompleted
                  : drivefs::mojom::ItemEvent::State::kInProgress,
        /*bytes_transferred*/ completed ? 100 : 50, /*bytes_to_transfer*/ 100,
        drivefs::mojom::ItemEventReason::kTransfer);
  }

  MOCK_METHOD1(PendingScreencastChangeCallback,
               void(const std::set<ash::PendingScreencast>&));

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
  {
    // Create a valid pending screencast.
    CreateFile(media_file);
    CreateFile(metadata_file);
  }
  drivefs::mojom::SyncingStatus syncing_status;
  AddTransferItemEvent(syncing_status, /*completed*/ false,
                       /*path*/ media_file);

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);
  content::RunAllTasksUntilIdle();

  const std::set<ash::PendingScreencast> pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(pending_screencasts.size(), 1);
  ash::PendingScreencast ps = *(pending_screencasts.begin());
  EXPECT_EQ(ps.container_dir, base::FilePath(kTestScreencastPath));
  EXPECT_EQ(ps.name, kTestScreencastName);

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
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest, InvalidScreencasts) {
  const std::string media_only_path = "/root/media_only/example.webm";
  const std::string metadata_only_path =
      "/root/metadata_only/example.projector";
  const std::string avi = "/root/non_screencast_files/example.avi";
  const std::string mov = "/root/non_screencast_files/example.mov";
  const std::string mp4 = "/root/non_screencast_files/example.mp4";

  {
    // Create an invalid screencast that only has webm medida file.
    CreateFile(media_only_path);

    // Create an invalid screencast that only has metadata file.
    CreateFile(metadata_only_path);

    // Create an invalid screencast that does not have webm media and metadata
    // files but have other media files.
    CreateFile(avi);
    CreateFile(mov);
    CreateFile(mp4);
  }

  drivefs::mojom::SyncingStatus syncing_status;
  AddTransferItemEvent(syncing_status, /*completed*/ false,
                       /*path*/ media_only_path);
  AddTransferItemEvent(syncing_status, /*completed*/ false,
                       /*path*/ metadata_only_path);
  AddTransferItemEvent(syncing_status, /*completed*/ false, /*path*/ avi);
  AddTransferItemEvent(syncing_status, /*completed*/ false, /*path*/ mov);
  AddTransferItemEvent(syncing_status, /*completed*/ false, /*path*/ mp4);

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
  {
    // Create a valid uploaded screencast.
    CreateFile(media_file);
    CreateFile(metadata_file);
  }

  drivefs::mojom::SyncingStatus syncing_status;
  AddTransferItemEvent(syncing_status, /*completed*/ true, /*path*/ media_file);

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(0);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       MultipleValidAndInvalidScreencasts) {
  drivefs::mojom::SyncingStatus syncing_status;
  int num_of_screencasts = 10;
  {
    // Create multiple valid pending screencasts.
    for (int i = 0; i < num_of_screencasts; ++i) {
      const std::string test_screencast_path =
          base::StrCat({kTestScreencastPath, base::NumberToString(i)});
      const std::string media =
          base::StrCat({test_screencast_path, "/", kTestMediaFile});
      const std::string metadata =
          base::StrCat({test_screencast_path, "/", kTestMetadataFile});
      CreateFile(media);
      CreateFile(metadata);

      AddTransferItemEvent(syncing_status, /*completed*/ false, /*path*/ media);
      AddTransferItemEvent(syncing_status, /*completed*/ false,
                           /*path*/ metadata);
    }

    // Tests with a invalid screencast does not have metadata file.
    const std::string no_metadata_screencast = "/root/no_metadata/example.webm";
    CreateFile(no_metadata_screencast);
    AddTransferItemEvent(syncing_status, /*completed*/ false,
                         /*path*/ no_metadata_screencast);

    // Tests with a invalid screencast does not have media file.
    const std::string no_media_screencast = "/root/no_media/example.projector";
    CreateFile(no_media_screencast);
    AddTransferItemEvent(syncing_status, /*completed*/ false,
                         /*path*/ no_media_screencast);

    // Tests with a non-screencast file.
    const std::string non_screencast = "/root/non_screencast/example.txt";
    CreateFile(non_screencast);
    AddTransferItemEvent(syncing_status, /*completed*/ false,
                         /*path*/ non_screencast);
  }

  EXPECT_CALL(*this, PendingScreencastChangeCallback(testing::_)).Times(1);
  pending_screencast_manager()->OnSyncingStatusUpdate(syncing_status);

  content::RunAllTasksUntilIdle();
  const std::set<ash::PendingScreencast> pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();

  // Only valid screencasts could be processed.
  EXPECT_EQ(pending_screencasts.size(), num_of_screencasts);
  for (int i = 0; i < num_of_screencasts; ++i) {
    const std::string container_dir =
        base::StrCat({kTestScreencastPath, base::NumberToString(i)});
    const std::string name =
        base::StrCat({kTestScreencastName, base::NumberToString(i)});
    ash::PendingScreencast ps{base::FilePath(container_dir), name};
    EXPECT_TRUE(pending_screencasts.find(ps) != pending_screencasts.end());
  }
}

}  // namespace ash
