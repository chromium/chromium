// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/test/mock_xhr_sender.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/ash/projector/projector_drivefs_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "content/public/test/browser_test.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace ash {
namespace {

constexpr char kTestScreencastPath[] = "/root/test_screencast";
constexpr char kTestScreencastName[] = "test_screencast";
constexpr char kTestMediaFile[] = "test_screencast.webm";
constexpr char kTestMetadataFile[] = "test_screencast.projector";
constexpr char kDefaultMetadataFilePath[] =
    "/root/test_screencast/test_screencast.projector";

constexpr char kProjectorPendingScreencastBatchIOTaskDurationHistogramName[] =
    "Ash.Projector.PendingScreencastBatchIOTaskDuration";
constexpr char kProjectorPendingScreencastChangeIntervalHistogramName[] =
    "Ash.Projector.PendingScreencastChangeInterval";

// The test media file is 0.7 mb.
constexpr int64_t kTestMediaFileBytes = 700 * 1024;
// The test metadata file is 0.1 mb.
constexpr int64_t kTestMetadataFileBytes = 100 * 1024;

}  // namespace

// Used to keep track of the count of OnScreencastsPendingStatusChanged call in
// PendingScreencastMangerBrowserTest.
class ScreencastsPendingStatusChangedObserver
    : public ProjectorAppClient::Observer {
 public:
  ScreencastsPendingStatusChangedObserver() {
    app_client_observation_.Observe(ProjectorAppClient::Get());
  }
  ~ScreencastsPendingStatusChangedObserver() override = default;

  int screencast_update_count() const { return screencast_update_count_; }

  // ProjectorAppClient::Observer:
  void OnScreencastsPendingStatusChanged(
      const PendingScreencastContainerSet& screencast_set) override {
    screencast_update_count_++;
  }
  void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& precondition) override {}
  void OnSodaProgress(int combined_progress) override {}
  void OnSodaError() override {}
  void OnSodaInstalled() override {}

 private:
  int screencast_update_count_ = 0;

  base::ScopedObservation<ProjectorAppClient, ProjectorAppClient::Observer>
      app_client_observation_{this};
};

class PendingScreencastMangerBrowserTest : public InProcessBrowserTest {
 public:
  PendingScreencastMangerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kProjectorUpdateIndexableText}, {});
  }
  PendingScreencastMangerBrowserTest(
      const PendingScreencastMangerBrowserTest&) = delete;
  PendingScreencastMangerBrowserTest& operator=(
      const PendingScreencastMangerBrowserTest&) = delete;
  ~PendingScreencastMangerBrowserTest() override = default;

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
    status_waiter_ =
        std::make_unique<ScreencastsPendingStatusChangedObserver>();
  }

  void TearDownOnMainThread() override {
    status_waiter_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore non-user profile.
    if (!ProfileHelper::IsUserProfile(profile)) {
      return nullptr;
    }

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");

    fake_drivefs_helper_ =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  drivefs::FakeDriveFs* GetFakeDriveFs() {
    return &fake_drivefs_helper_->fake_drivefs();
  }

  // Creates file under the Drive relative `file_path` whose size is
  // `total_bytes`.
  void CreateFileInDriveFsFolder(const std::string& file_path,
                                 int64_t total_bytes) {
    CreateFileInDriveFsFolder(file_path, std::string(total_bytes, 'a'));
  }

  // Creates file under the Drive relative `file_path` and write `file_content`
  // to the file.
  void CreateFileInDriveFsFolder(const std::string& file_path,
                                 const std::string& file_content) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath relative_file_path(file_path);
    base::FilePath folder_path =
        GetDriveFsAbsolutePath(relative_file_path.DirName().value());

    // base::CreateDirectory returns 'true' on successful creation, or if the
    // directory already exists.
    EXPECT_TRUE(base::CreateDirectory(folder_path));

    base::File file(folder_path.Append(relative_file_path.BaseName()),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    EXPECT_EQ(static_cast<int>(file_content.size()),
              file.Write(/*offset=*/0, file_content.data(),
                         /*size=*/file_content.size()));
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
        std::in_place, /*stable_id=*/1, /*group_id=*/1, path,
        total_bytes == transferred_bytes
            ? drivefs::mojom::ItemEvent::State::kCompleted
            : drivefs::mojom::ItemEvent::State::kInProgress,
        /*bytes_transferred=*/transferred_bytes,
        /*bytes_to_transfer=*/total_bytes,
        drivefs::mojom::ItemEventReason::kTransfer);
  }

  // Notifies `pending_screencast_manager_` that a file with `file_path` and
  // `total_size` with an uploading event and a completed event.
  void MockSyncFileCompleted(const std::string& file_path,
                             const int64_t total_size) {
    drivefs::mojom::SyncingStatus syncing_status;
    // Notifies with uploading event:
    AddTransferItemEvent(syncing_status, file_path,
                         /*total_bytes=*/0,
                         /*transferred_bytes=*/total_size);
    SimulateSyncingEvent(syncing_status);
    WaitForPendingStatusUpdateToBeFinished();
    syncing_status.item_events.clear();

    // Notifies with completed event:
    AddTransferItemEvent(syncing_status, file_path,
                         /*total_bytes=*/total_size,
                         /*transferred_bytes=*/total_size);
    SimulateSyncingEvent(syncing_status);
    WaitForPendingStatusUpdateToBeFinished();
  }

  void TestGetFileIdFailed() {
    // Sets get file id callback:
    base::RunLoop run_loop;
    pending_screencast_manager()->SetOnGetFileIdCallbackForTest(
        base::BindLambdaForTesting([&](const base::FilePath& local_file_path,
                                       const std::string& file_id) {
          EXPECT_EQ(GetDriveFsAbsolutePath(kDefaultMetadataFilePath),
                    local_file_path);
          EXPECT_EQ(std::string(), file_id);
          run_loop.Quit();
        }));

    // Mocks a metadata file finishes upload:
    MockSyncFileCompleted(kDefaultMetadataFilePath, kTestMetadataFileBytes);
    run_loop.Run();
  }

  void ExpectEmptyRequestBodyForProjectorFileContent(
      const std::string& file_content) {
    CreateFileInDriveFsFolder(kDefaultMetadataFilePath, file_content);
    drivefs::FakeMetadata metadata;
    metadata.path = base::FilePath(kDefaultMetadataFilePath);
    metadata.mime_type = "text/plain";
    metadata.original_name = kTestMetadataFile;
    metadata.doc_id = "abc123";
    metadata.alternate_url = "https://drive.google.com/open?id=fileId";
    GetFakeDriveFs()->SetMetadata(std::move(metadata));

    // Sets get file id callback:
    base::RunLoop run_loop;
    pending_screencast_manager()->SetOnGetRequestBodyCallbackForTest(
        base::BindLambdaForTesting(
            [&](const std::string& file_id, const std::string& request_body) {
              EXPECT_EQ(std::string(), request_body);
              EXPECT_EQ("fileId", file_id);
              run_loop.Quit();
            }));

    // Mocks a metadata file finishes upload:
    MockSyncFileCompleted(kDefaultMetadataFilePath, kTestMetadataFileBytes);

    run_loop.Run();
  }

  void VerifyNotificationCount(size_t size) {
    base::RunLoop run_loop;
    NotificationDisplayServiceFactory::GetForProfile(browser()->profile())
        ->GetDisplayed(base::BindLambdaForTesting(
            [&run_loop, &size](std::set<std::string> displayed_notification_ids,
                               bool supports_synchronization) {
              EXPECT_EQ(size, displayed_notification_ids.size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Simulates syncing event by FakeDriveFs delegate.
  void SimulateSyncingEvent(
      const drivefs::mojom::SyncingStatus& syncing_status) {
    auto& drivefs_delegate = GetFakeDriveFs()->delegate();
    drivefs_delegate->OnSyncingStatusUpdate(syncing_status.Clone());
    drivefs_delegate.FlushForTesting();
  }

  void WaitForPendingStatusUpdateToBeFinished() {
    // Ensures
    // PendingScreencastManager::ProcessAndGenerateNewScreencasts finishes on
    // blocking task runner.
    WaitBlockingTaskRunnerFinish();
    // Ensures
    // PendingScreencastManager::OnProcessAndGenerateNewScreencastsFinished
    // finishes on blocking task runner.
    WaitBlockingTaskRunnerFinish();
  }

  PendingScreencastManager* pending_screencast_manager() {
    ProjectorAppClientImpl* app_client =
        static_cast<ProjectorAppClientImpl*>(ash::ProjectorAppClient::Get());
    return app_client->get_pending_screencast_manager_for_test();
  }

  ScreencastsPendingStatusChangedObserver* status_waiter() {
    return status_waiter_.get();
  }

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  void WaitBlockingTaskRunnerFinish() {
    base::RunLoop run_loop;
    pending_screencast_manager()->blocking_task_runner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  std::unique_ptr<ScreencastsPendingStatusChangedObserver> status_waiter_;
};

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest, ValidScreencast) {
  const std::string media_file =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create a valid pending screencast.
    CreateFileAndTransferItemEvent(media_file,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    CreateFileAndTransferItemEvent(kDefaultMetadataFilePath,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
  }

  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(1, status_waiter()->screencast_update_count());

  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastChangeIntervalHistogramName,
      /*count=*/0);
  const base::TimeTicks last_pending_screencast_change_tick =
      pending_screencast_manager()->last_pending_screencast_change_tick();
  EXPECT_NE(base::TimeTicks(), last_pending_screencast_change_tick);

  const PendingScreencastContainerSet pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(pending_screencasts.size(), 1u);
  ash::PendingScreencastContainer ps = *(pending_screencasts.begin());
  EXPECT_EQ(ps.container_dir(), base::FilePath(kTestScreencastPath));
  EXPECT_EQ(ps.pending_screencast().name, kTestScreencastName);
  EXPECT_EQ(ps.pending_screencast().created_time,
            GetFileCreatedTime(media_file)
                .InMillisecondsFSinceUnixEpochIgnoringNull());

  // Tests PendingScreencastChangeCallback won't be invoked if pending
  // screencast status doesn't change.
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(1, status_waiter()->screencast_update_count());

  // Expects no report since PendingScreencastChangeCallback wasn't invoked.
  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastChangeIntervalHistogramName,
      /*count=*/0);
  EXPECT_EQ(
      last_pending_screencast_change_tick,
      pending_screencast_manager()->last_pending_screencast_change_tick());

  // Tests PendingScreencastChangeCallback will be invoked if pending
  // screencast status changes.
  syncing_status.item_events.clear();
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(2, status_waiter()->screencast_update_count());

  // Since pending screencast set is empty, the last pending screencast change
  // tick is reset to null:
  EXPECT_EQ(
      base::TimeTicks(),
      pending_screencast_manager()->last_pending_screencast_change_tick());

  const base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_pending_screencast_change_tick;
  auto change_interval_samples = histogram_tester_.GetAllSamples(
      kProjectorPendingScreencastChangeIntervalHistogramName);
  // Expects only 1 sample.
  EXPECT_EQ(1u, change_interval_samples.size());
  // Expects the sample only have 1 count.
  EXPECT_EQ(1, change_interval_samples.front().count);
  // Since the end of `elapsed_time` is gotten from "base::TimeTicks::Now()"
  // after PendingScreencastChangeCallback gets invoked. Expects `elapsed_time`
  // is greater than the sample.
  EXPECT_GT(elapsed_time.InMicroseconds(), change_interval_samples.front().min);
  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName,
      /*count=*/3);
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

  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(0, status_waiter()->screencast_update_count());

  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName,
      /*count=*/1);
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       IgnoreCompletedEvent) {
  const std::string media_file =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create a valid uploaded screencast.
    CreateFileAndTransferItemEvent(media_file,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   kTestMediaFileBytes, syncing_status);
    CreateFileAndTransferItemEvent(kDefaultMetadataFilePath,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   kTestMetadataFileBytes, syncing_status);
  }

  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(0, status_waiter()->screencast_update_count());

  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());

  // There is no IO task for complete events.
  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName,
      /*count=*/0);
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

  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(1, status_waiter()->screencast_update_count());

  const PendingScreencastContainerSet pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  int64_t total_size = kTestMediaFileBytes + kTestMetadataFileBytes;

  // Only valid screencasts could be processed.
  EXPECT_EQ(pending_screencasts.size(), num_of_screencasts);
  for (size_t i = 0; i < num_of_screencasts; ++i) {
    const std::string container_dir =
        base::StrCat({kTestScreencastPath, base::NumberToString(i)});
    const std::string name =
        base::StrCat({kTestScreencastName, base::NumberToString(i)});
    ash::PendingScreencastContainer ps{base::FilePath(container_dir), name,
                                       total_size, 0};
    EXPECT_TRUE(pending_screencasts.find(ps) != pending_screencasts.end());
  }
  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName,
      /*count=*/1);
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest, UploadProgress) {
  const std::string media_file_path =
      base::StrCat({kTestScreencastPath, "/", kTestMediaFile});
  drivefs::mojom::SyncingStatus syncing_status;
  {
    // Create a valid pending screencast.
    CreateFileAndTransferItemEvent(media_file_path,
                                   /*total_bytes=*/kTestMediaFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
    CreateFileAndTransferItemEvent(kDefaultMetadataFilePath,
                                   /*total_bytes=*/kTestMetadataFileBytes,
                                   /*transferred_bytes=*/0, syncing_status);
  }

  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(1, status_waiter()->screencast_update_count());

  const PendingScreencastContainerSet pending_screencasts_1 =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(pending_screencasts_1.size(), 1u);
  ash::PendingScreencastContainer ps = *(pending_screencasts_1.begin());
  const int total_size = kTestMediaFileBytes + kTestMetadataFileBytes;
  EXPECT_EQ(total_size, ps.total_size_in_bytes());
  EXPECT_EQ(0, ps.bytes_transferred());

  // Tests the metadata file finished transferred.
  // PendingScreencastChangeCallback won't be invoked if the difference is less
  // than kPendingScreencastDiffThresholdInBytes.
  syncing_status.item_events.clear();
  int64_t media_transferred_1_bytes = 1;
  int64_t metadata_transferred_bytes = kTestMetadataFileBytes;
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/media_transferred_1_bytes);
  // Create a completed transferred event for metadata.
  AddTransferItemEvent(syncing_status, kDefaultMetadataFilePath,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/metadata_transferred_bytes);
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(1, status_waiter()->screencast_update_count());

  const PendingScreencastContainerSet pending_screencasts_2 =
      pending_screencast_manager()->GetPendingScreencasts();
  ps = *(pending_screencasts_2.begin());
  // The screencast status unchanged.
  EXPECT_EQ(total_size, ps.total_size_in_bytes());
  EXPECT_EQ(0, ps.bytes_transferred());

  syncing_status.item_events.clear();
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/kTestMediaFileBytes - 1);
  // Create a completed transferred event for metadata.
  AddTransferItemEvent(syncing_status, kDefaultMetadataFilePath,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/metadata_transferred_bytes);
  // Tests PendingScreencastChangeCallback will be invoked if the difference of
  // transferred bytes is greater than kPendingScreencastDiffThresholdInBytes.
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(2, status_waiter()->screencast_update_count());

  const PendingScreencastContainerSet pending_screencasts_3 =
      pending_screencast_manager()->GetPendingScreencasts();
  ps = *(pending_screencasts_3.begin());
  // The screencast status changed.
  EXPECT_EQ(total_size, ps.total_size_in_bytes());

  // TODO(b/209854146) After fix b/209854146, the `ps.bytes_transferred` is
  // `total_size -1`.
  EXPECT_EQ(kTestMediaFileBytes - 1, ps.bytes_transferred());

  syncing_status.item_events.clear();
  // Create completed transferred events for both files.
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/kTestMediaFileBytes);
  AddTransferItemEvent(syncing_status, kDefaultMetadataFilePath,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/kTestMetadataFileBytes);
  // Tests PendingScreencastChangeCallback will be invoked when all files
  // finished transferred.
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  EXPECT_EQ(3, status_waiter()->screencast_update_count());

  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());
  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName,
      /*count=*/4);
}

// Test the comparison of pending screencast in a std::set.
IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       PendingScreencastContainerSet) {
  // The `name` and `total_size_in_bytes` of screencast will not be compare in a
  // set.
  const base::FilePath container_dir_a = base::FilePath("/root/a");
  const std::string screencast_a_name = "a";
  const int64_t screencast_a_total_bytes = 2 * 1024 * 1024;
  ash::PendingScreencastContainer screencast_a_1_byte_transferred{
      container_dir_a, screencast_a_name, screencast_a_total_bytes,
      /*bytes_transferred=*/1};
  ash::PendingScreencastContainer screencast_a_1kb_transferred{
      container_dir_a, screencast_a_name, screencast_a_total_bytes,
      /*bytes_transferred=*/1024};
  ash::PendingScreencastContainer screencast_a_700kb_transferred{
      container_dir_a, screencast_a_name, screencast_a_total_bytes,
      /*bytes_transferred=*/700 * 1024};

  const base::FilePath container_dir_b = base::FilePath("/root/b");
  const std::string screencast_b_name = "b";
  const int64_t screencast_b_total_bytes = 2 * 1024 * 1024;
  ash::PendingScreencastContainer screencast_b_1_byte_transferred{
      container_dir_b, screencast_b_name, screencast_b_total_bytes,
      /*bytes_transferred=*/1};
  ash::PendingScreencastContainer screencast_b_1kb_transferred{
      container_dir_b, screencast_b_name, screencast_b_total_bytes,
      /*bytes_transferred=*/1024};
  ash::PendingScreencastContainer screencast_b_700kb_transferred{
      container_dir_b, screencast_b_name, screencast_b_total_bytes,
      /*bytes_transferred=*/700 * 1024};

  PendingScreencastContainerSet set1{screencast_a_1_byte_transferred,
                                     screencast_b_1_byte_transferred};
  PendingScreencastContainerSet set2{screencast_a_1_byte_transferred,
                                     screencast_b_1_byte_transferred};
  PendingScreencastContainerSet set3{screencast_a_1kb_transferred,
                                     screencast_b_1_byte_transferred};
  PendingScreencastContainerSet set4{screencast_a_700kb_transferred,
                                     screencast_b_1_byte_transferred};
  PendingScreencastContainerSet set5{screencast_a_1_byte_transferred,
                                     screencast_a_700kb_transferred};
  PendingScreencastContainerSet set6{screencast_a_700kb_transferred,
                                     screencast_a_1_byte_transferred};
  PendingScreencastContainerSet set7{screencast_a_1_byte_transferred,
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
  drivefs::mojom::SyncingStatus syncing_status;
  // Create a valid pending screencast.
  CreateFileAndTransferItemEvent(media_file_path,
                                 /*total_bytes=*/kTestMediaFileBytes,
                                 /*transferred_bytes=*/0, syncing_status);
  CreateFileAndTransferItemEvent(kDefaultMetadataFilePath,
                                 /*total_bytes=*/kTestMetadataFileBytes,
                                 /*transferred_bytes=*/0, syncing_status);

  // Mock DriveFs sends an out of space error for media file.
  drivefs::mojom::DriveError error{
      drivefs::mojom::DriveError::Type::kCantUploadStorageFull,
      base::FilePath(media_file_path)};
  pending_screencast_manager()->OnError(error);

  // Even there's DriveError, DriveFs will keep trying to sync both metadata and
  // media file.
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();

  // Verify we have a fail status screencast.
  const PendingScreencastContainerSet pending_screencasts =
      pending_screencast_manager()->GetPendingScreencasts();
  EXPECT_EQ(1u, pending_screencasts.size());
  ash::PendingScreencastContainer ps = *(pending_screencasts.begin());
  EXPECT_TRUE(ps.pending_screencast().upload_failed);

  // Mock both metadata and media file get uploaded.
  syncing_status.item_events.clear();
  // Create completed transferred events for both files.
  AddTransferItemEvent(syncing_status, media_file_path,
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/kTestMediaFileBytes);
  AddTransferItemEvent(syncing_status, kDefaultMetadataFilePath,
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/kTestMetadataFileBytes);
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();

  // Expect the screencast get removed from pending screencasts set .
  EXPECT_TRUE(pending_screencast_manager()->GetPendingScreencasts().empty());

  histogram_tester_.ExpectTotalCount(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName,
      /*count=*/2);
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       UpdateIndexableTextSuccess) {
  // Prepares a ".projector" file and it's metadata:
  const std::string kProjectorFileContent =
      R"({
        "captionLanguage": "en",
        "captions": [
          {"endOffset": 400, "startOffset": 200, "editState": 1},
          {"endOffset": 1260, "hypothesisParts": [], "startOffset": 760,
          "text": "metadata file."},
          {"endOffset": 2300, "hypothesisParts": [], "startOffset": 2000,
          "text": "another sentence."}
        ],
        "tableOfContent":[]})";
  CreateFileInDriveFsFolder(kDefaultMetadataFilePath, kProjectorFileContent);
  drivefs::FakeMetadata metadata;
  metadata.path = base::FilePath(kDefaultMetadataFilePath);
  metadata.mime_type = "text/plain";
  metadata.original_name = kTestMetadataFile;
  metadata.doc_id = "abc123";
  metadata.alternate_url = "https://drive.google.com/open?id=fileId";
  GetFakeDriveFs()->SetMetadata(std::move(metadata));

  // Sets get file id callback:
  base::RunLoop run_loop;
  network::TestURLLoaderFactory test_url_loader_factory;
  pending_screencast_manager()->SetProjectorXhrSenderForTest(
      std::make_unique<MockXhrSender>(
          base::BindLambdaForTesting(
              [&](const GURL& url, projector::mojom::RequestType method,
                  const std::optional<std::string>& request_body) {
                EXPECT_EQ(
                    "{\"contentHints\":{\"indexableText\":\" metadata file. "
                    "another sentence.\"}}",
                    *request_body);
                EXPECT_EQ(projector::mojom::RequestType::kPatch, method);
                EXPECT_EQ(
                    GURL("https://www.googleapis.com/drive/v3/files/fileId"),
                    url);
                run_loop.Quit();
              }),
          &test_url_loader_factory));

  // Mocks a metadata file finishes upload:
  MockSyncFileCompleted(kDefaultMetadataFilePath, kTestMetadataFileBytes);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       UpdateIndexableTextFailByEmptyFileId) {
  // Does not create ".projector", which leads to drive::FILE_ERROR_NOT_FOUND.

  TestGetFileIdFailed();
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       UpdateIndexableTextFailByEmptyAlternateUrl) {
  CreateFileInDriveFsFolder(kDefaultMetadataFilePath, kTestMetadataFileBytes);
  // Sets empty alternate url in metadata, which could happen when metadata is
  // not fully populated.
  drivefs::FakeMetadata metadata;
  metadata.path = base::FilePath(kDefaultMetadataFilePath);
  metadata.mime_type = "text/plain";
  metadata.original_name = kTestMetadataFile;
  metadata.doc_id = "abc123";
  GetFakeDriveFs()->SetMetadata(std::move(metadata));

  TestGetFileIdFailed();
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       UpdateIndexableTextFailByInCorrectAlternateUrl) {
  CreateFileInDriveFsFolder(kDefaultMetadataFilePath, kTestMetadataFileBytes);
  // Sets incorrect alternate url in metadata.
  drivefs::FakeMetadata metadata;
  metadata.path = base::FilePath(kDefaultMetadataFilePath);
  metadata.mime_type = "text/plain";
  metadata.original_name = kTestMetadataFile;
  metadata.doc_id = "abc123";
  metadata.alternate_url = "alternate_url";
  GetFakeDriveFs()->SetMetadata(std::move(metadata));

  TestGetFileIdFailed();
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       MalformedProjectorFileNoCaption) {
  // Prepares a ".projector" file with no captions.
  const std::string kProjectorFileContentNoCaption =
      "{\"captionLanguage\":\"en\",\"tableOfContent\":[]}";
  ExpectEmptyRequestBodyForProjectorFileContent(kProjectorFileContentNoCaption);
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       MalformedProjectorFileNotJson) {
  // Prepares a ".projector" file with no captions.
  const std::string kProjectorFileContentNotJson =
      "{\"captionLanguage\":\"en\",\"captions\":[{\"endOffset\":1260,"
      "\"hypothesisParts\":[],\"startOffset\":760,\"text\":\"metadata "
      "file.\"}],\"tableOfContent\":[]";
  ExpectEmptyRequestBodyForProjectorFileContent(kProjectorFileContentNotJson);
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       ProjectorFileEmptyCaption) {
  // Prepares a ".projector" file and it's metadata:
  const std::string kProjectorFileContentEmptyCaption =
      "{\"captionLanguage\":\"en\",\"captions\":[],\"tableOfContent\":[]}";
  ExpectEmptyRequestBodyForProjectorFileContent(
      kProjectorFileContentEmptyCaption);
}

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerBrowserTest,
                       SuppressDriveNotification) {
  auto* app_client = ProjectorAppClient::Get();
  app_client->NotifyAppUIActive(true);

  base::FilePath container_folder = base::FilePath(kTestScreencastPath);
  const base::FilePath media_file = container_folder.Append(kTestMediaFile);
  const base::FilePath metadata_file =
      container_folder.Append(kTestMetadataFile);
  const base::FilePath thumbnail =
      container_folder.Append(kScreencastDefaultThumbnailFileName);
  const base::FilePath drivefs_mounted_point =
      ProjectorDriveFsProvider::GetDriveFsMountPointPath();
  app_client->ToggleFileSyncingNotificationForPaths(
      {GetDriveFsAbsolutePath(media_file.value()),
       GetDriveFsAbsolutePath(metadata_file.value()),
       GetDriveFsAbsolutePath(thumbnail.value())},
      true);

  drivefs::mojom::SyncingStatus syncing_status;
  AddTransferItemEvent(syncing_status, media_file.value(),
                       /*total_bytes=*/kTestMediaFileBytes,
                       /*transferred_bytes=*/0);
  AddTransferItemEvent(syncing_status, metadata_file.value(),
                       /*total_bytes=*/kTestMetadataFileBytes,
                       /*transferred_bytes=*/0);
  AddTransferItemEvent(syncing_status, thumbnail.value(),
                       /*total_bytes=*/300,
                       /*transferred_bytes=*/0);

  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  VerifyNotificationCount(0);

  // When app is closed, the notification shows up:
  app_client->NotifyAppUIActive(false);
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  VerifyNotificationCount(0);

  // When app is open, the notification gets suppressed again:
  app_client->NotifyAppUIActive(true);
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  VerifyNotificationCount(0);

  // While app is open, the Drive notification shows up for non-projector file.
  CreateFileAndTransferItemEvent("unrelated file",
                                 /*total_bytes=*/100,
                                 /*transferred_bytes=*/0, syncing_status);
  SimulateSyncingEvent(syncing_status);
  WaitForPendingStatusUpdateToBeFinished();
  VerifyNotificationCount(0);
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
        std::make_unique<PendingScreencastManager>(base::BindLambdaForTesting(
            [&](const PendingScreencastContainerSet& set) {
              base::DoNothing();
            }));
  }

  void TearDownOnMainThread() override {
    pending_screencast_manager_.reset();
    LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  std::unique_ptr<PendingScreencastManager> pending_screencast_manager_;
};

IN_PROC_BROWSER_TEST_F(PendingScreencastMangerMultiProfileTest,
                       SwitchActiveUser) {
  LoginUser(account_id1_);

  // Verify DriveFsHost observation is observing user 1's DriveFsHost.
  Profile* profile1 = ProfileHelper::Get()->GetProfileByAccountId(account_id1_);
  drive::DriveIntegrationService* service_for_account1 =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile1);
  EXPECT_EQ(pending_screencast_manager_->GetHost(),
            service_for_account1->GetDriveFsHost());

  // Add user 2.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  // Verify DriveFsHost observation is observing user 2's DriveFsHost.
  Profile* profile2 = ProfileHelper::Get()->GetProfileByAccountId(account_id2_);
  drive::DriveIntegrationService* service_for_account2 =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile2);
  EXPECT_EQ(pending_screencast_manager_->GetHost(),
            service_for_account2->GetDriveFsHost());

  // Switch back to user1.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  // Verify DriveFsHost observation is observing user 1's DriveFsHost.
  EXPECT_EQ(pending_screencast_manager_->GetHost(),
            service_for_account1->GetDriveFsHost());
}

}  // namespace ash
