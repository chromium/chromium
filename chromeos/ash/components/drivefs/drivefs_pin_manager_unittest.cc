// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
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
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;

// Shorthand way to represent drive files with the information that is relevant
// for the pinning manager.
struct DriveItem {
  static int64_t counter;
  int64_t stable_id = ++counter;
  int64_t size = 0;
  base::FilePath path;
  mojom::FileMetadata::Type type = mojom::FileMetadata::Type::kFile;
  bool pinned = false;
  bool available_offline = false;
  // Whether to send a status update for this drive item. If false this will get
  // filtered out when converting `DriveItem` in `MakeSyncingStatus`.
  bool status_update = true;
};

int64_t DriveItem::counter = 0;

mojom::FileMetadataPtr MakeMetadata(const bool available_offline,
                                    const int64_t size) {
  mojom::FileMetadataPtr md = mojom::FileMetadata::New();
  md->available_offline = available_offline;
  md->size = size;
  return md;
}

mojom::FileMetadataPtr MakeMetadata(const DriveItem& item) {
  mojom::FileMetadataPtr md = mojom::FileMetadata::New();
  md->stable_id = item.stable_id;
  md->type = item.type;
  md->size = item.size;
  md->pinned = item.pinned;
  md->available_offline = item.available_offline;
  md->capabilities = mojom::Capabilities::New();
  return md;
}

// An action that takes a `std::vector<DriveItem>` and is used to update the
// items that are returned via the `GetNextPage` callback. These shorthand items
// are converted to mojo types that represent the actual types returned.
// NOTE: `arg0` in the below represents the pointer passed via parameters to the
// `MOCK_METHOD` of `OnGetNextPage`.
ACTION_P(PopulateSearchItems, items) {
  std::vector<mojom::QueryItemPtr> result;
  result.reserve(items.size());
  for (const DriveItem& item : items) {
    mojom::QueryItemPtr p = mojom::QueryItem::New();
    p->path = item.path;
    p->metadata = MakeMetadata(item);
    result.push_back(std::move(p));
  }
  *arg0 = std::move(result);
}

// An action that populates no search results. This is required as the final
// `GetNextPage` query will return 0 items and this ensures the `MOCK_METHOD`
// returns the appropriate type (instead of `absl::nullopt`).
ACTION(PopulateNoSearchItems) {
  *arg0 = std::vector<mojom::QueryItemPtr>();
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), error, std::move(items)));
  }

  MOCK_METHOD(void,
              SetPinned,
              (const base::FilePath&,
               bool,
               base::OnceCallback<void(drive::FileError)>),
              (override));

  MOCK_METHOD(void,
              SetPinnedByStableId,
              (int64_t, bool, base::OnceCallback<void(drive::FileError)>),
              (override));

  MOCK_METHOD(
      void,
      GetMetadata,
      (const base::FilePath&,
       base::OnceCallback<void(drive::FileError, mojom::FileMetadataPtr)>),
      (override));

  MOCK_METHOD(
      void,
      GetMetadataByStableId,
      (int64_t,
       base::OnceCallback<void(drive::FileError, mojom::FileMetadataPtr)>),
      (override));

 private:
  mojo::Receiver<mojom::SearchQuery> search_receiver_{this};
};

class MockFreeSpace {
 public:
  MOCK_METHOD(void,
              GetFreeSpace,
              (const base::FilePath&, DriveFsPinManager::SpaceResult));
};

}  // namespace

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

  static mojom::SyncingStatusPtr MakeSyncingStatus(
      const std::vector<DriveItem>& items,
      mojom::ItemEvent::State state = mojom::ItemEvent::State::kQueued) {
    mojom::SyncingStatusPtr status = mojom::SyncingStatus::New();

    std::vector<mojom::ItemEventPtr> events;
    for (const DriveItem& item : items) {
      if (item.pinned || !item.status_update) {
        continue;
      }
      mojom::ItemEventPtr event = mojom::ItemEvent::New();
      event->stable_id = item.stable_id;
      event->path = item.path.value();
      event->state = state;
      event->bytes_to_transfer = item.size;
      events.push_back(std::move(event));
    }

    status->item_events = std::move(events);
    return status;
  }

  static void SetState(std::vector<mojom::ItemEventPtr>& events,
                       const mojom::ItemEvent::State state) {
    for (mojom::ItemEventPtr& event : events) {
      DCHECK(event);
      event->state = state;
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath gcache_dir_;
  MockFreeSpace mock_free_space_;
  MockDriveFs mock_drivefs_;
};

// Tests DriveFsPinManagerTest::Add().
TEST_F(DriveFsPinManagerTest, Add) {
  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.transferred_bytes, 0);
    EXPECT_EQ(progress.total_bytes, 0);
    EXPECT_EQ(progress.required_space, 0);
  }

  const DriveFsPinManager::StableId id1 = DriveFsPinManager::StableId(549);
  const std::string path1 = "Path 1";
  const int64_t size1 = 698248964;

  const DriveFsPinManager::StableId id2 = DriveFsPinManager::StableId(17);
  const std::string path2 = "Path 2";
  const int64_t size2 = 78964533;

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Add an item.
  EXPECT_TRUE(manager.Add(id1, path1, size1));
  EXPECT_THAT(manager.files_to_pin_, SizeIs(1));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Try to add a conflicting item with the same ID, but different path and
  // size.
  EXPECT_FALSE(manager.Add(id1, path2, size2));
  EXPECT_THAT(manager.files_to_pin_, SizeIs(1));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const auto it = manager.files_to_pin_.find(id1);
    ASSERT_NE(it, manager.files_to_pin_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, size1);
    EXPECT_EQ(progress.transferred, 0);
    EXPECT_FALSE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.transferred_bytes, 0);
    EXPECT_EQ(progress.total_bytes, size1);
    EXPECT_EQ(progress.required_space, 698249216);
  }

  // Add a second item.
  EXPECT_TRUE(manager.Add(id2, path2, size2));
  EXPECT_THAT(manager.files_to_pin_, SizeIs(2));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const auto it = manager.files_to_pin_.find(id2);
    ASSERT_NE(it, manager.files_to_pin_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id2);
    EXPECT_EQ(progress.path, path2);
    EXPECT_EQ(progress.total, size2);
    EXPECT_EQ(progress.transferred, 0);
    EXPECT_FALSE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.transferred_bytes, 0);
    EXPECT_EQ(progress.total_bytes, size1 + size2);
    EXPECT_EQ(progress.required_space, 777216000);
  }
}

TEST_F(DriveFsPinManagerTest, CannotGetFreeSpace) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(0);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_)).Times(0);
  EXPECT_CALL(mock_callback, Run(SetupStage::kCannotCalculateFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kCannotCalculateFreeSpace);
  EXPECT_EQ(progress.free_space, 0);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.transferred_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, CannotListFiles) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_FAILED)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kCannotRetrieveSearchResults))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kCannotRetrieveSearchResults);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.transferred_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, InvalidFileList) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(Return(drive::FileError::FILE_ERROR_OK));
  EXPECT_CALL(mock_callback, Run(SetupStage::kCannotRetrieveSearchResults))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kCannotRetrieveSearchResults);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.transferred_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, NotEnoughSpace) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;
  base::RunLoop run_loop;

  // Mock Drive search to return 3 unpinned files that total just above 512 MB.
  // The available space of 1 GB is not enough if you take in account the 512 MB
  // margin.
  const std::vector<DriveItem> items = {
      {.size = 300 << 20}, {.size = 212 << 20}, {.size = 1}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, (512 << 20) + (4 << 10));
  EXPECT_EQ(progress.transferred_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, JustCheckRequiredSpace) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;
  base::RunLoop run_loop;

  // Mock Drive search to return 2 unpinned files that total to 512 MB. The
  // available space of 1 GB is just enough if you take in account the 512 MB
  // margin.
  const std::vector<DriveItem> items = {{.size = 300 << 20},
                                        {.size = 212 << 20}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));

  // Just check the required space. Don't try to pin any file.
  const bool should_pin = false;
  manager.Start(mock_callback.Get(), should_pin);
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kSuccess);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 512 << 20);
  EXPECT_EQ(progress.transferred_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest,
       DISABLED_FailingToPinOneItemShouldNotFailCompletely) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  const std::vector<DriveItem> items = {{.size = 128}, {.size = 128}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, don't return a
      // final empty list as this should be aborted due to one of the pinning
      // operations being mock failed.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      // Mock the first file to successfully get pinned.
      .WillOnce(RunOnceCallback<2>(drive::FILE_ERROR_OK))
      // Mock the second file to unsuccessfully get pinned.
      .WillOnce(RunOnceCallback<2>(drive::FILE_ERROR_FAILED));

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, DISABLED_OnlyUnpinnedItemsShouldGetPinned) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  std::vector<DriveItem> items = {
      {.size = 128, .path = base::FilePath("/a")},
      {.size = 128, .path = base::FilePath("/b")},
      {.size = 128, .path = base::FilePath("/c"), .pinned = true}};

  ON_CALL(mock_drivefs_, GetMetadata(_, _))
      .WillByDefault(
          [&items](
              const base::FilePath& path,
              base::OnceCallback<void(drive::FileError, mojom::FileMetadataPtr)>
                  callback) {
            for (const DriveItem& item : items) {
              if (item.path == path) {
                std::move(callback).Run(drive::FILE_ERROR_OK,
                                        MakeMetadata(item));
                return;
              }
            }
            std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
          });

  EXPECT_CALL(mock_drivefs_, GetMetadata(_, _)).Times(0);

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
  EXPECT_CALL(mock_drivefs_, SetPinnedByStableId(items[0].stable_id, true, _))
      .WillOnce([&items](int64_t, bool,
                         base::OnceCallback<void(drive::FileError)> callback) {
        items[0].pinned = true;
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), drive::FILE_ERROR_OK));
      });
  EXPECT_CALL(mock_drivefs_, SetPinnedByStableId(items[1].stable_id, true, _))
      .WillOnce([&items](int64_t, bool,
                         base::OnceCallback<void(drive::FileError)> callback) {
        items[1].pinned = true;
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), drive::FILE_ERROR_OK));
      });
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();

  {
    const mojom::SyncingStatusPtr status =
        MakeSyncingStatus(items, mojom::ItemEvent::State::kQueued);
    manager.OnSyncingStatusUpdate(*status);
  }

  {
    const mojom::SyncingStatusPtr status =
        MakeSyncingStatus(items, mojom::ItemEvent::State::kInProgress);
    manager.OnSyncingStatusUpdate(*status);
  }

  {
    const mojom::SyncingStatusPtr status =
        MakeSyncingStatus(items, mojom::ItemEvent::State::kCompleted);
    manager.OnSyncingStatusUpdate(*status);
  }
}

TEST_F(DriveFsPinManagerTest,
       DISABLED_ZeroByteItemsAndHostedItemsShouldBePeriodicallyCleaned) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  base::FilePath gdoc_path("/a.gdoc");
  base::FilePath b_path("/b");
  const std::vector<DriveItem> items = {
      // The `a.gdoc` file will never receive an `OnSyncingStatusUpdate` and
      // thus needs to be removed via the periodic removal task.
      {.size = 0, .path = gdoc_path, .status_update = false},
      {.size = 128, .path = b_path}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
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

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.Start(mock_callback.Get());
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  const mojom::SyncingStatusPtr status = MakeSyncingStatus(items);
  manager.OnSyncingStatusUpdate(*status);

  // Flipping all the events to `kCompleted` will not start the next search
  // query as the `a.gdoc` file is still remaining in the syncing items. As the
  // task environment was started with a mock time, the `base::Runloop` will
  // execute all tasks then automatically advance the clock until the periodic
  // removal task is executed, cleaning the "a.gdoc" file.
  base::RunLoop new_run_loop;
  EXPECT_CALL(mock_drivefs_, GetMetadata(b_path, _))
      .WillOnce(RunOnceCallback<1>(
          drive::FILE_ERROR_OK,
          MakeMetadata(/*available_offline=*/true, /*size=*/128)));
  EXPECT_CALL(mock_drivefs_, GetMetadata(gdoc_path, _))
      // Mock the first file to be available offline with a 0 size.
      .WillOnce(RunOnceCallback<1>(
          drive::FILE_ERROR_OK,
          MakeMetadata(/*available_offline=*/true, /*size=*/0)));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  SetState(status->item_events, mojom::ItemEvent::State::kCompleted);
  manager.OnSyncingStatusUpdate(*status);
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
       DISABLED_SyncingStatusUpdateProgressIsReportedBackToObserver) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  base::FilePath file_path("/b");
  const std::vector<DriveItem> items = {{.size = 128, .path = file_path}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(drive::FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(DoAll(PopulateSearchItems(items),
                      Return(drive::FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
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

  DriveFsPinManager manager(
      temp_dir_.GetPath(), &mock_drivefs_,
      base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                          base::Unretained(&mock_free_space_)));
  manager.AddObserver(&mock_pin_observer);
  manager.Start(mock_callback.Get());
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  const mojom::SyncingStatusPtr status = MakeSyncingStatus(items);
  manager.OnSyncingStatusUpdate(*status);

  // Update the item in the syncing status to have transferred 10 bytes and
  // expect the progress to return that information.
  base::RunLoop setup_progress_run_loop;
  SetState(status->item_events, mojom::ItemEvent::State::kInProgress);
  status->item_events.at(0)->bytes_transferred = 10;
  EXPECT_CALL(mock_pin_observer,
              OnSetupProgress(
                  AllOf(Field(&SetupProgress::transferred_bytes, 10),
                        Field(&SetupProgress::stage, SetupStage::kSyncing))))
      .Times(1)
      .WillOnce(RunClosure(setup_progress_run_loop.QuitClosure()));
  manager.OnSyncingStatusUpdate(*status);
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
          MakeMetadata(/*available_offline=*/true, /*size=*/128)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  SetState(status->item_events, mojom::ItemEvent::State::kCompleted);
  status->item_events.at(0)->bytes_transferred = 128;
  EXPECT_CALL(mock_pin_observer,
              OnSetupProgress(
                  AllOf(Field(&SetupProgress::transferred_bytes, 128),
                        Field(&SetupProgress::stage, SetupStage::kSuccess))))
      .Times(1)
      .WillOnce(RunClosure(setup_progress_run_loop.QuitClosure()));
  manager.OnSyncingStatusUpdate(*status);
  new_run_loop.Run();
}

}  // namespace drivefs::pinning
