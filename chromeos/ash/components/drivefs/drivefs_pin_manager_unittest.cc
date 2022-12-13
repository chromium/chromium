// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs::pinning {

namespace {

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Return;

// Shorthand way to represent drive files with the information that is relevant
// for the pinning manager.
struct DriveItem {
  int64_t size;
  base::FilePath path;
  bool pinned;
  // Whether to send a status update for this drive item, if false this will get
  // filtered out when converting `DriveItem` in `CreateSyncingStatusUpdate`.
  bool status_update = true;
};

// An action that takes a `std::vector<DriveItem>` and is used to update the
// items that are returned via the `GetNextPage` callback. These shorthand items
// are converted to mojo types that represent the actual types returned.
// NOTE: `arg0` in the below represents the pointer passed via parameters to the
// `MOCK_METHOD` of `OnGetNextPage`.
ACTION_P(PopulateSearchItems, drive_items) {
  std::vector<mojom::QueryItemPtr> items;
  for (const auto& item : drive_items) {
    items.emplace_back(mojom::QueryItem::New());
    items.back()->metadata = mojom::FileMetadata::New();
    items.back()->metadata->capabilities = mojom::Capabilities::New();
    items.back()->metadata->size = item.size;
    items.back()->metadata->pinned = item.pinned;
    items.back()->path = item.path;
  }
  *arg0 = std::move(items);
}

// An action that populates no search results. This is required as the final
// `GetNextPage` query will return 0 items and this ensures the `MOCK_METHOD`
// returns the appropriate type (instead of `absl::nullopt`).
ACTION(PopulateNoSearchItems) {
  std::vector<mojom::QueryItemPtr> items;
  *arg0 = std::move(items);
}

class MockDriveFs : public mojom::DriveFsInterceptorForTesting,
                    public mojom::SearchQuery {
 public:
  MockDriveFs() = default;

  MockDriveFs(const MockDriveFs&) = delete;
  MockDriveFs& operator=(const MockDriveFs&) = delete;

  mojom::DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MOCK_METHOD(void, OnStartSearchQuery, (const mojom::QueryParameters&));

  void StartSearchQuery(mojo::PendingReceiver<mojom::SearchQuery> receiver,
                        mojom::QueryParametersPtr query_params) override {
    search_receiver_.reset();
    OnStartSearchQuery(*query_params);
    search_receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD(drive::FileError,
              OnGetNextPage,
              (absl::optional<std::vector<mojom::QueryItemPtr>> * items));

  void GetNextPage(GetNextPageCallback callback) override {
    absl::optional<std::vector<mojom::QueryItemPtr>> items;
    auto error = OnGetNextPage(&items);
    std::move(callback).Run(error, std::move(items));
  }

  MOCK_METHOD(void,
              SetPinned,
              (const base::FilePath&,
               bool,
               base::OnceCallback<void(drive::FileError)>),
              (override));

  MOCK_METHOD(
      void,
      GetMetadata,
      (const base::FilePath&,
       base::OnceCallback<void(drive::FileError, mojom::FileMetadataPtr)>),
      (override));

 private:
  mojo::Receiver<mojom::SearchQuery> search_receiver_{this};
};

class MockFreeDiskSpaceImpl : public FreeDiskSpaceDelegate {
 public:
  MockFreeDiskSpaceImpl() = default;

  MockFreeDiskSpaceImpl(const MockFreeDiskSpaceImpl&) = delete;
  MockFreeDiskSpaceImpl& operator=(const MockFreeDiskSpaceImpl&) = delete;

  ~MockFreeDiskSpaceImpl() override = default;

  MOCK_METHOD(void,
              AmountOfFreeDiskSpace,
              (const base::FilePath&, base::OnceCallback<void(int64_t)>),
              (override));
};

class DriveFsPinManagerTest : public testing::Test {
 public:
  DriveFsPinManagerTest() = default;

  DriveFsPinManagerTest(const DriveFsPinManagerTest&) = delete;
  DriveFsPinManagerTest& operator=(const DriveFsPinManagerTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    gcache_dir_ = temp_dir_.GetPath().Append("GCache");
  }

  mojom::SyncingStatusPtr CreateSyncingStatusUpdate(
      const std::vector<DriveItem> items) {
    mojom::SyncingStatusPtr status = mojom::SyncingStatus::New();

    std::vector<mojom::ItemEventPtr> item_events;
    for (const auto& item : items) {
      if (item.pinned || !item.status_update) {
        continue;
      }
      mojom::ItemEventPtr item_event = mojom::ItemEvent::New();
      item_event->path = item.path.value();
      item_event->state = mojom::ItemEvent::State::kQueued;
      item_event->bytes_to_transfer = item.size;
      item_events.push_back(std::move(item_event));
    }

    status->item_events = std::move(item_events);
    return status;
  }

  void ChangeAllItemEventsToState(std::vector<mojom::ItemEventPtr>& item_events,
                                  mojom::ItemEvent::State state) {
    for (auto& item : item_events) {
      item->state = state;
    }
  }

  mojom::FileMetadataPtr CreateFileMetadataItem(bool available_offline,
                                                int64_t size) {
    auto metadata_item = mojom::FileMetadata::New();
    metadata_item->available_offline = available_offline;
    metadata_item->size = size;
    return metadata_item;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath gcache_dir_;
  MockDriveFs mock_drivefs_;
};

TEST_F(DriveFsPinManagerTest, DisabledPinManagerShouldNotStartSearching) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(0);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_)).Times(0);
  EXPECT_CALL(mock_callback, Run(SetupError::kManagerDisabled))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(_, _)).Times(0);

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/false, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, OnFreeDiskSpaceFailingShouldNotSearchDrive) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(0);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_)).Times(0);
  EXPECT_CALL(mock_callback, Run(SetupError::kErrorCalculatingFreeDiskSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, DriveReturningAnErrorShouldFail) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_FAILED)));
  EXPECT_CALL(mock_callback, Run(SetupError::kErrorRetrievingSearchResults))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, DriveReturnedSuccessButInvalidResultsShouldFail) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(Return(drive::FileError::FILE_ERROR_OK));
  EXPECT_CALL(mock_callback, Run(SetupError::kErrorResultsReturnedInvalid))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, IfPinnedItemSizeExceedsFreeDiskSpaceShouldFail) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  // Mock Drive search to return 2 unpinned files that total to 1.5 MB which
  // exceeds the mocked available space of 1 MB.
  std::vector<DriveItem> expected_drive_items = {{.size = 768}, {.size = 768}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupError::kErrorNotEnoughFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, FailingToPinOneItemShouldFailCompletely) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  std::vector<DriveItem> expected_drive_items = {{.size = 128}, {.size = 128}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, don't return a
      // final empty list as this should be aborted due to one of the pinning
      // operations being mock failed.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupError::kErrorFailedToPinItem))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      // Mock the first file to successfully get pinned.
      .WillOnce(RunOnceCallback<2>(drive::FILE_ERROR_OK))
      // Mock the second file to unsuccessfully get pinned.
      .WillOnce(RunOnceCallback<2>(drive::FILE_ERROR_FAILED));

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, OnlyUnpinnedItemsShouldGetPinned) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  std::vector<DriveItem> expected_drive_items = {
      {.size = 128, .path = base::FilePath("/a")},
      {.size = 128, .path = base::FilePath("/b")},
      {.size = 128, .path = base::FilePath("/c"), .pinned = true}};

  // The `PeriodicallyRemoveUnpinnedItems` will get ran when the task queue is
  // idle so ensure the `GetMetadata` call returns values that enable the flow
  // to continue.
  ON_CALL(mock_drivefs_, GetMetadata(_, _))
      .WillByDefault(RunOnceCallback<1>(
          drive::FILE_ERROR_OK,
          CreateFileMetadataItem(/*available_offline=*/true, /*size=*/128)));

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      .Times(2)
      .WillOnce(RunOnceCallback<2>(drive::FILE_ERROR_OK))
      // `RunOnceCallback` can't be chained together in a `DoAll` action
      // combinator, so use an inline lambda instead.
      .WillOnce(
          [&run_loop](const base::FilePath& path, bool pinned,
                      base::OnceCallback<void(drive::FileError)> callback) {
            std::move(callback).Run(drive::FILE_ERROR_OK);
            run_loop.QuitClosure().Run();
          });

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  mojom::SyncingStatusPtr status =
      CreateSyncingStatusUpdate(expected_drive_items);
  manager->OnSyncingStatusUpdate(*status);

  // When all items are in progress, they should not start iterating over the
  // next search page.
  ChangeAllItemEventsToState(status->item_events,
                             mojom::ItemEvent::State::kInProgress);
  manager->OnSyncingStatusUpdate(*status);

  // Flipping all the events to `kCompleted` should then start the next query.
  // By populating no search items this indicates the end of the available items
  // and thus it finished.
  base::RunLoop new_run_loop;
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupError::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  EXPECT_CALL(mock_drivefs_, GetMetadata(_, _))
      .Times(2)
      .WillOnce(RunOnceCallback<1>(drive::FILE_ERROR_OK,
                                   CreateFileMetadataItem(true, 128)))
      .WillOnce(RunOnceCallback<1>(drive::FILE_ERROR_OK,
                                   CreateFileMetadataItem(true, 128)));
  ChangeAllItemEventsToState(status->item_events,
                             mojom::ItemEvent::State::kCompleted);
  manager->OnSyncingStatusUpdate(*status);
  new_run_loop.Run();
}

TEST_F(
    DriveFsPinManagerTest,
    ZeroByteItemsAndHostedItemsShouldBePeriodicallyCleanedFromTheInProgressMap) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  base::FilePath gdoc_path("/a.gdoc");
  base::FilePath b_path("/b");
  std::vector<DriveItem> expected_drive_items = {
      // The `a.gdoc` file will never receive an `OnSyncingStatusUpdate` and
      // thus needs to be removed via the periodic removal task.
      {.size = 0, .path = gdoc_path, .status_update = false},
      {.size = 128, .path = b_path}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      .Times(2)
      .WillOnce(RunOnceCallback<2>(drive::FILE_ERROR_OK))
      // `RunOnceCallback` can't be chained together in a `DoAll` action
      // combinator, so use an inline lambda instead.
      .WillOnce(
          [&run_loop](const base::FilePath& path, bool pinned,
                      base::OnceCallback<void(drive::FileError)> callback) {
            std::move(callback).Run(drive::FILE_ERROR_OK);
            run_loop.QuitClosure().Run();
          });

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->Start(mock_callback.Get());
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  mojom::SyncingStatusPtr status =
      CreateSyncingStatusUpdate(expected_drive_items);
  manager->OnSyncingStatusUpdate(*status);

  // Flipping all the events to `kCompleted` will not start the next search
  // query as the `a.gdoc` file is still remaining in the syncing items. As the
  // task environment was started with a mock time, the `base::Runloop` will
  // execute all tasks then automatically advance the clock until the periodic
  // removal task is executed, cleaning the "a.gdoc" file.
  base::RunLoop new_run_loop;
  EXPECT_CALL(mock_drivefs_, GetMetadata(b_path, _))
      .WillOnce(RunOnceCallback<1>(
          drive::FILE_ERROR_OK,
          CreateFileMetadataItem(/*available_offline=*/true, /*size=*/128)));
  EXPECT_CALL(mock_drivefs_, GetMetadata(gdoc_path, _))
      // Mock the first file to be available offline with a 0 size.
      .WillOnce(RunOnceCallback<1>(
          drive::FILE_ERROR_OK,
          CreateFileMetadataItem(/*available_offline=*/true, /*size=*/0)));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupError::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  ChangeAllItemEventsToState(status->item_events,
                             mojom::ItemEvent::State::kCompleted);
  manager->OnSyncingStatusUpdate(*status);
  new_run_loop.Run();
}

class TestBulkPinObserver : public DriveFsBulkPinObserver {
 public:
  TestBulkPinObserver() = default;

  TestBulkPinObserver(const TestBulkPinObserver&) = delete;
  TestBulkPinObserver& operator=(const TestBulkPinObserver&) = delete;

  ~TestBulkPinObserver() override = default;

  MOCK_METHOD(void, OnSetupProgress, (const SetupProgress&), (override));
};

TEST_F(DriveFsPinManagerTest,
       SyncingStatusUpdateProgressIsReportedBackToObserver) {
  base::MockOnceCallback<void(SetupError)> mock_callback;
  auto mock_free_disk_space = std::make_unique<MockFreeDiskSpaceImpl>();

  base::RunLoop run_loop;

  base::FilePath file_path("/b");
  std::vector<DriveItem> expected_drive_items = {
      {.size = 128, .path = file_path}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(DoAll(PopulateSearchItems(expected_drive_items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(*mock_free_disk_space, AmountOfFreeDiskSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1024));  // 1 MB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      .Times(1)
      // `RunOnceCallback` can't be chained together in a `DoAll` action
      // combinator, so use an inline lambda instead.
      .WillOnce(
          [&run_loop](const base::FilePath& path, bool pinned,
                      base::OnceCallback<void(drive::FileError)> callback) {
            std::move(callback).Run(drive::FILE_ERROR_OK);
            run_loop.QuitClosure().Run();
          });

  TestBulkPinObserver mock_pin_observer;
  EXPECT_CALL(mock_pin_observer, OnSetupProgress(_)).Times(AnyNumber());

  auto manager = std::make_unique<DriveFsPinManager>(
      /*enabled=*/true, temp_dir_.GetPath(), &mock_drivefs_,
      std::move(mock_free_disk_space));
  manager->AddObserver(&mock_pin_observer);
  manager->Start(mock_callback.Get());
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  mojom::SyncingStatusPtr status =
      CreateSyncingStatusUpdate(expected_drive_items);
  manager->OnSyncingStatusUpdate(*status);

  // Update the item in the syncing status to have transferred 10 bytes and
  // expect the progress to return that information.
  base::RunLoop setup_progress_run_loop;
  ChangeAllItemEventsToState(status->item_events,
                             mojom::ItemEvent::State::kInProgress);
  status->item_events.at(0)->bytes_transferred = 10;
  EXPECT_CALL(
      mock_pin_observer,
      OnSetupProgress(AllOf(Field(&SetupProgress::pinned_disk_space, 10),
                            Field(&SetupProgress::stage,
                                  SetupStage::kCalculatedRequiredDiskSpace))))
      .Times(1)
      .WillOnce(RunClosure(setup_progress_run_loop.QuitClosure()));
  manager->OnSyncingStatusUpdate(*status);
  setup_progress_run_loop.Run();

  // Flip all the items to `kCompleted` and move the `bytes_transferred` size to
  // be the total size of the file. The reported progress should only add the
  // delta so we expect the pinned disk space to only equal the final file size.
  base::RunLoop new_run_loop;
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_drivefs_, GetMetadata(_, _))
      .WillOnce(RunOnceCallback<1>(
          drive::FILE_ERROR_OK,
          CreateFileMetadataItem(/*available_offline=*/true, /*size=*/128)));
  EXPECT_CALL(mock_callback, Run(SetupError::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  ChangeAllItemEventsToState(status->item_events,
                             mojom::ItemEvent::State::kCompleted);
  status->item_events.at(0)->bytes_transferred = 128;
  EXPECT_CALL(mock_pin_observer,
              OnSetupProgress(AllOf(
                  Field(&SetupProgress::pinned_disk_space, 128),
                  Field(&SetupProgress::stage, SetupStage::kFinishedSetup))))
      .Times(1)
      .WillOnce(RunClosure(setup_progress_run_loop.QuitClosure()));
  manager->OnSyncingStatusUpdate(*status);
  new_run_loop.Run();
}

}  // namespace

}  // namespace drivefs::pinning
