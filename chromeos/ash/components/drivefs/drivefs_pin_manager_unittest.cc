// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

using base::BindOnce;
using base::FilePath;
using base::OnceCallback;
using base::SequencedTaskRunner;
using base::test::RunClosure;
using base::test::RunOnceCallback;
using drive::FileError;
using mojom::FileMetadata;
using mojom::FileMetadataPtr;
using mojom::ItemEvent;
using mojom::ItemEventPtr;
using mojom::QueryItem;
using mojom::QueryItemPtr;
using mojom::SearchQuery;
using mojom::SyncingStatus;
using mojom::SyncingStatusPtr;
using std::string;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Field;
using testing::IsEmpty;
using testing::Return;
using testing::SizeIs;

using StableId = DriveFsPinManager::StableId;

// Shorthand way to represent drive files with the information that is relevant
// for the pinning manager.
struct DriveItem {
  static int64_t counter;
  int64_t stable_id = ++counter;
  int64_t size = 0;
  FilePath path;
  FileMetadata::Type type = FileMetadata::Type::kFile;
  bool pinned = false;
  bool available_offline = false;
  // Whether to send a status update for this drive item. If false this will get
  // filtered out when converting `DriveItem` in `MakeSyncingStatus`.
  bool status_update = true;
};

int64_t DriveItem::counter = 0;

FileMetadataPtr MakeMetadata(const bool available_offline, const int64_t size) {
  FileMetadataPtr md = FileMetadata::New();
  md->available_offline = available_offline;
  md->size = size;
  return md;
}

FileMetadataPtr MakeMetadata(const DriveItem& item) {
  FileMetadataPtr md = FileMetadata::New();
  md->stable_id = item.stable_id;
  md->type = item.type;
  md->size = item.size;
  md->pinned = item.pinned;
  md->available_offline = item.available_offline;
  md->capabilities = mojom::Capabilities::New();
  return md;
}

// An action that takes a `vector<DriveItem>` and is used to update the items
// that are returned via the `GetNextPage` callback. These shorthand items are
// converted to mojo types that represent the actual types returned. NOTE:
// `arg0` in the below represents the pointer passed via parameters to the
// `MOCK_METHOD` of `OnGetNextPage`.
ACTION_P(PopulateSearchItems, items) {
  vector<QueryItemPtr> result;
  result.reserve(items.size());
  for (const DriveItem& item : items) {
    QueryItemPtr p = QueryItem::New();
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
  *arg0 = vector<QueryItemPtr>();
}

class MockDriveFs : public mojom::DriveFsInterceptorForTesting,
                    public SearchQuery {
 public:
  MockDriveFs() = default;

  MockDriveFs(const MockDriveFs&) = delete;
  MockDriveFs& operator=(const MockDriveFs&) = delete;

  mojom::DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MOCK_METHOD(void, OnStartSearchQuery, (const mojom::QueryParameters&));

  void StartSearchQuery(mojo::PendingReceiver<SearchQuery> receiver,
                        mojom::QueryParametersPtr query_params) override {
    search_receiver_.reset();
    OnStartSearchQuery(*query_params);
    search_receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD(FileError,
              OnGetNextPage,
              (absl::optional<vector<QueryItemPtr>> * items));

  void GetNextPage(GetNextPageCallback callback) override {
    absl::optional<vector<QueryItemPtr>> items;
    auto error = OnGetNextPage(&items);
    SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(std::move(callback), error, std::move(items)));
  }

  MOCK_METHOD(void,
              SetPinned,
              (const FilePath&, bool, OnceCallback<void(FileError)>),
              (override));

  MOCK_METHOD(void,
              SetPinnedByStableId,
              (int64_t, bool, OnceCallback<void(FileError)>),
              (override));

  MOCK_METHOD(void,
              GetMetadata,
              (const FilePath&, OnceCallback<void(FileError, FileMetadataPtr)>),
              (override));

  MOCK_METHOD(void,
              GetMetadataByStableId,
              (int64_t, OnceCallback<void(FileError, FileMetadataPtr)>),
              (override));

 private:
  mojo::Receiver<SearchQuery> search_receiver_{this};
};

class MockFreeSpace {
 public:
  MOCK_METHOD(void,
              GetFreeSpace,
              (const FilePath&, DriveFsPinManager::SpaceResult));
};

class MockObserver : public DriveFsPinManager::Observer {
 public:
  MOCK_METHOD(void, OnProgress, (const SetupProgress&), (override));
  MOCK_METHOD(void, OnDrop, (), (override));
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

  static SyncingStatusPtr MakeSyncingStatus(
      const vector<DriveItem>& items,
      ItemEvent::State state = ItemEvent::State::kQueued) {
    SyncingStatusPtr status = SyncingStatus::New();

    vector<ItemEventPtr> events;
    for (const DriveItem& item : items) {
      if (item.pinned || !item.status_update) {
        continue;
      }
      ItemEventPtr event = ItemEvent::New();
      event->stable_id = item.stable_id;
      event->path = item.path.value();
      event->state = state;
      event->bytes_to_transfer = item.size;
      events.push_back(std::move(event));
    }

    status->item_events = std::move(events);
    return status;
  }

  static void SetState(vector<ItemEventPtr>& events,
                       const ItemEvent::State state) {
    for (ItemEventPtr& event : events) {
      DCHECK(event);
      event->state = state;
    }
  }

  DriveFsPinManager::SpaceGetter GetSpaceGetter() {
    return base::BindRepeating(&MockFreeSpace::GetFreeSpace,
                               base::Unretained(&mock_free_space_));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  FilePath gcache_dir_;
  MockFreeSpace mock_free_space_;
  MockDriveFs mock_drivefs_;
};

// Tests DriveFsPinManagerTest::Add().
TEST_F(DriveFsPinManagerTest, Add) {
  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 0);
    EXPECT_EQ(progress.required_space, 0);
  }

  const DriveFsPinManager::StableId id1 = DriveFsPinManager::StableId(549);
  const string path1 = "Path 1";
  const int64_t size1 = 698248964;

  const DriveFsPinManager::StableId id2 = DriveFsPinManager::StableId(17);
  const string path2 = "Path 2";
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
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, size1);
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
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, size1 + size2);
    EXPECT_EQ(progress.required_space, 777216000);
  }
}

// Tests DriveFsPinManagerTest::Update().
TEST_F(DriveFsPinManagerTest, Update) {
  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.pinned_bytes = 5000;
  manager.progress_.bytes_to_pin = 10000;
  manager.progress_.required_space = 20480;

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  const DriveFsPinManager::StableId id1 = DriveFsPinManager::StableId(549);
  const string path1 = "Path 1";
  const int64_t size1 = 2000;

  const DriveFsPinManager::StableId id2 = DriveFsPinManager::StableId(17);
  const string path2 = "Path 2";
  const int64_t size2 = 5000;

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, DriveFsPinManager::Progress{.path = path1, .total = size1});
    ASSERT_TRUE(ok);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Try to update an unknown file.
  EXPECT_FALSE(manager.Update(id2, path2, size2, size2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
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
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // Mark file as in progress.
  EXPECT_TRUE(manager.Update(id1, path1, -1, -1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, size1);
    EXPECT_EQ(progress.transferred, 0);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // These updates should not modify anything.
  EXPECT_FALSE(manager.Update(id1, path1, -1, -1));
  EXPECT_FALSE(manager.Update(id1, path1, 0, -1));
  EXPECT_FALSE(manager.Update(id1, path1, -1, size1));
  EXPECT_FALSE(manager.Update(id1, path1, 0, size1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, size1);
    EXPECT_EQ(progress.transferred, 0);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // Update total size.
  EXPECT_TRUE(manager.Update(id1, path1, -1, size2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, size2);
    EXPECT_EQ(progress.transferred, 0);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 24576);
  }

  // Update transferred bytes.
  EXPECT_TRUE(manager.Update(id1, path1, size1, -1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, size2);
    EXPECT_EQ(progress.transferred, size1);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 7000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 24576);
  }

  // Update path.
  EXPECT_TRUE(manager.Update(id1, path2, -1, -1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path2);
    EXPECT_EQ(progress.total, size2);
    EXPECT_EQ(progress.transferred, size1);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 7000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 24576);
  }

  // Progress goes backwards.
  EXPECT_TRUE(manager.Update(id1, path2, 1000, -1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path2);
    EXPECT_EQ(progress.total, size2);
    EXPECT_EQ(progress.transferred, 1000);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 6000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 24576);
  }
}

// Tests DriveFsPinManagerTest::Remove().
TEST_F(DriveFsPinManagerTest, Remove) {
  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.pinned_bytes = 5000;
  manager.progress_.bytes_to_pin = 10000;
  manager.progress_.required_space = 20480;

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  const DriveFsPinManager::StableId id1 = DriveFsPinManager::StableId(549);
  const string path1 = "Path 1";

  const DriveFsPinManager::StableId id2 = DriveFsPinManager::StableId(17);
  const string path2 = "Path 2";

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, DriveFsPinManager::Progress{.path = path1,
                                         .transferred = 1200,
                                         .total = 3000,
                                         .in_progress = true});
    ASSERT_TRUE(ok);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Try to remove an unknown file.
  EXPECT_FALSE(manager.Remove(id2, path2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, 3000);
    EXPECT_EQ(progress.transferred, 1200);
    EXPECT_TRUE(progress.in_progress);
  }

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // Remove file with default final size.
  EXPECT_TRUE(manager.Remove(id1, path2));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 6800);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, DriveFsPinManager::Progress{.path = path1,
                                         .transferred = 1200,
                                         .total = 3000,
                                         .in_progress = true});
    ASSERT_TRUE(ok);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Remove file while setting size to zero.
  EXPECT_TRUE(manager.Remove(id1, path2, 0));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5600);
    EXPECT_EQ(progress.bytes_to_pin, 7000);
    EXPECT_EQ(progress.required_space, 16384);
  }

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, DriveFsPinManager::Progress{.path = path1,
                                         .transferred = 5000,
                                         .total = 6000,
                                         .in_progress = true});
    ASSERT_TRUE(ok);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Remove file while setting size to a different value that the expected one.
  EXPECT_TRUE(manager.Remove(id1, path1, 10000));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 10600);
    EXPECT_EQ(progress.bytes_to_pin, 11000);
    EXPECT_EQ(progress.required_space, 20480);
  }
}

// Tests DriveFsPinManagerTest::OnSyncingEvent().
TEST_F(DriveFsPinManagerTest, OnSyncingEvent) {
  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.bytes_to_pin = 30000;
  manager.progress_.required_space = 32768;

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  const DriveFsPinManager::StableId id1 = DriveFsPinManager::StableId(549);
  const string path1 = "Path 1";

  const DriveFsPinManager::StableId id2 = DriveFsPinManager::StableId(17);
  const string path2 = "Path 2";

  // Put in place a couple of files to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, DriveFsPinManager::Progress{.path = path1, .total = 10000});
    ASSERT_TRUE(ok);
  }
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id2, DriveFsPinManager::Progress{.path = path2, .total = 20000});
    ASSERT_TRUE(ok);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  // An event with an unknown type is ignored.
  {
    ItemEvent event;
    event.stable_id = static_cast<int64_t>(id2);
    event.path = path2;
    event.state = ItemEvent::State(-1);
    event.bytes_to_transfer = -1;
    event.bytes_transferred = -1;
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  // Mark file 1 as queued.
  {
    ItemEvent event;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1;
    event.state = ItemEvent::State::kQueued;
    event.bytes_to_transfer = 0;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, 10000);
    EXPECT_EQ(progress.transferred, 0);
    EXPECT_TRUE(progress.in_progress);
  }

  // Mark file 1 as in progress.
  {
    ItemEvent event;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1;
    event.state = ItemEvent::State::kInProgress;
    event.bytes_to_transfer = 10000;
    event.bytes_transferred = 5000;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, progress] = *it;
    EXPECT_EQ(got_id, id1);
    EXPECT_EQ(progress.path, path1);
    EXPECT_EQ(progress.total, 10000);
    EXPECT_EQ(progress.transferred, 5000);
    EXPECT_TRUE(progress.in_progress);
  }

  // Mark file 1 as completed.
  {
    ItemEvent event;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1;
    event.state = ItemEvent::State::kCompleted;
    event.bytes_to_transfer = -1;
    event.bytes_transferred = -1;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 10000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    EXPECT_EQ(it, manager.files_to_track_.end());
  }

  // Mark file 2 as failed.
  {
    ItemEvent event;
    event.stable_id = static_cast<int64_t>(id2);
    event.path = path2;
    event.state = ItemEvent::State::kFailed;
    event.bytes_to_transfer = -1;
    event.bytes_transferred = -1;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const SetupProgress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 1);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 10000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 12288);
  }

  {
    const auto it = manager.files_to_track_.find(id2);
    EXPECT_EQ(it, manager.files_to_track_.end());
  }
}

TEST_F(DriveFsPinManagerTest, CannotGetFreeSpace) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(0);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_)).Times(0);
  EXPECT_CALL(mock_callback, Run(SetupStage::kCannotGetFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kCannotGetFreeSpace);
  EXPECT_EQ(progress.free_space, 0);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, CannotListFiles) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_FAILED)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kCannotListFiles))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kCannotListFiles);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, InvalidFileList) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_OK));
  EXPECT_CALL(mock_callback, Run(SetupStage::kCannotListFiles))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kCannotListFiles);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, NotEnoughSpace) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;
  base::RunLoop run_loop;

  // Mock Drive search to return 3 unpinned files that total just above 512 MB.
  // The available space of 1 GB is not enough if you take in account the 512 MB
  // margin.
  const vector<DriveItem> items = {
      {.size = 300 << 20}, {.size = 212 << 20}, {.size = 1}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, (512 << 20) + (4 << 10));
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, JustCheckRequiredSpace) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;
  base::RunLoop run_loop;

  // Mock Drive search to return 2 unpinned files that total to 512 MB. The
  // available space of 1 GB is just enough if you take in account the 512 MB
  // margin.
  const vector<DriveItem> items = {{.size = 300 << 20}, {.size = 212 << 20}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.ShouldPin(false);
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  const SetupProgress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, SetupStage::kSuccess);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 512 << 20);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest,
       DISABLED_FailingToPinOneItemShouldNotFailCompletely) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  const vector<DriveItem> items = {{.size = 128}, {.size = 128}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, don't return a
      // final empty list as this should be aborted due to one of the pinning
      // operations being mock failed.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      // Mock the first file to successfully get pinned.
      .WillOnce(RunOnceCallback<2>(FileError::FILE_ERROR_OK))
      // Mock the second file to unsuccessfully get pinned.
      .WillOnce(RunOnceCallback<2>(FileError::FILE_ERROR_FAILED));

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, DISABLED_OnlyUnpinnedItemsShouldGetPinned) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  vector<DriveItem> items = {
      {.size = 128, .path = FilePath("/a")},
      {.size = 128, .path = FilePath("/b")},
      {.size = 128, .path = FilePath("/c"), .pinned = true}};

  ON_CALL(mock_drivefs_, GetMetadata(_, _))
      .WillByDefault(
          [&items](const FilePath& path,
                   OnceCallback<void(FileError, FileMetadataPtr)> callback) {
            for (const DriveItem& item : items) {
              if (item.path == path) {
                std::move(callback).Run(FileError::FILE_ERROR_OK,
                                        MakeMetadata(item));
                return;
              }
            }
            std::move(callback).Run(FileError::FILE_ERROR_NOT_FOUND, nullptr);
          });

  EXPECT_CALL(mock_drivefs_, GetMetadata(_, _)).Times(0);

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
  EXPECT_CALL(mock_drivefs_, SetPinnedByStableId(items[0].stable_id, true, _))
      .WillOnce([&items](int64_t, bool,
                         OnceCallback<void(FileError)> callback) {
        items[0].pinned = true;
        SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, BindOnce(std::move(callback), FileError::FILE_ERROR_OK));
      });
  EXPECT_CALL(mock_drivefs_, SetPinnedByStableId(items[1].stable_id, true, _))
      .WillOnce([&items](int64_t, bool,
                         OnceCallback<void(FileError)> callback) {
        items[1].pinned = true;
        SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, BindOnce(std::move(callback), FileError::FILE_ERROR_OK));
      });
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  {
    const SyncingStatusPtr status =
        MakeSyncingStatus(items, ItemEvent::State::kQueued);
    manager.OnSyncingStatusUpdate(*status);
  }

  {
    const SyncingStatusPtr status =
        MakeSyncingStatus(items, ItemEvent::State::kInProgress);
    manager.OnSyncingStatusUpdate(*status);
  }

  {
    const SyncingStatusPtr status =
        MakeSyncingStatus(items, ItemEvent::State::kCompleted);
    manager.OnSyncingStatusUpdate(*status);
  }
}

TEST_F(DriveFsPinManagerTest,
       DISABLED_ZeroByteItemsAndHostedItemsShouldBePeriodicallyCleaned) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  FilePath gdoc_path("/a.gdoc");
  FilePath b_path("/b");
  const vector<DriveItem> items = {
      // The `a.gdoc` file will never receive an `OnSyncingStatusUpdate` and
      // thus needs to be removed via the periodic removal task.
      {.size = 0, .path = gdoc_path, .status_update = false},
      {.size = 128, .path = b_path}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      .Times(2)
      .WillOnce(RunOnceCallback<2>(FileError::FILE_ERROR_OK))
      // `RunOnceCallback` can't be chained together in a `DoAll` action
      // combinator, so use an inline lambda instead.
      .WillOnce([&run_loop](const FilePath& path, bool pinned,
                            OnceCallback<void(FileError)> callback) {
        std::move(callback).Run(FileError::FILE_ERROR_OK);
        run_loop.QuitClosure().Run();
      });

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  const SyncingStatusPtr status = MakeSyncingStatus(items);
  manager.OnSyncingStatusUpdate(*status);

  // Flipping all the events to `kCompleted` will not start the next search
  // query as the `a.gdoc` file is still remaining in the syncing items. As the
  // task environment was started with a mock time, the `base::Runloop` will
  // execute all tasks then automatically advance the clock until the periodic
  // removal task is executed, cleaning the "a.gdoc" file.
  base::RunLoop new_run_loop;
  EXPECT_CALL(mock_drivefs_, GetMetadata(b_path, _))
      .WillOnce(RunOnceCallback<1>(
          FileError::FILE_ERROR_OK,
          MakeMetadata(/*available_offline=*/true, /*size=*/128)));
  EXPECT_CALL(mock_drivefs_, GetMetadata(gdoc_path, _))
      // Mock the first file to be available offline with a 0 size.
      .WillOnce(RunOnceCallback<1>(
          FileError::FILE_ERROR_OK,
          MakeMetadata(/*available_offline=*/true, /*size=*/0)));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  SetState(status->item_events, ItemEvent::State::kCompleted);
  manager.OnSyncingStatusUpdate(*status);
  new_run_loop.Run();
}

TEST_F(DriveFsPinManagerTest, OnDrop) {
  {
    MockObserver observer;
    DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
    manager.AddObserver(&observer);
    EXPECT_CALL(observer, OnDrop()).Times(1);
  }
  {
    MockObserver observer;
    EXPECT_CALL(observer, OnDrop()).Times(0);
    DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
    manager.AddObserver(&observer);
    manager.RemoveObserver(&observer);
  }
}

TEST_F(DriveFsPinManagerTest,
       DISABLED_SyncingStatusUpdateProgressIsReportedBackToObserver) {
  base::MockOnceCallback<void(SetupStage)> mock_callback;

  base::RunLoop run_loop;

  FilePath file_path("/b");
  const vector<DriveItem> items = {{.size = 128, .path = file_path}};

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_)).Times(2);
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      // Results returned whilst calculating free disk space.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)))
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)))
      // Results returned when actually performing the pinning, the final
      // response (i.e. PopulateNoSearchItems()) happens after the
      // `OnSyncingStatusUpdate` instead.
      .WillOnce(
          DoAll(PopulateSearchItems(items), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_free_space_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.
  EXPECT_CALL(mock_drivefs_, SetPinned(_, true, _))
      .Times(1)
      // `RunOnceCallback` can't be chained together in a `DoAll` action
      // combinator, so use an inline lambda instead.
      .WillOnce([&run_loop](const FilePath& path, bool pinned,
                            OnceCallback<void(FileError)> callback) {
        std::move(callback).Run(FileError::FILE_ERROR_OK);
        run_loop.QuitClosure().Run();
      });

  MockObserver observer;
  EXPECT_CALL(observer, OnProgress(_)).Times(AnyNumber());

  DriveFsPinManager manager(temp_dir_.GetPath(), &mock_drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.AddObserver(&observer);
  manager.SetCompletionCallback(mock_callback.Get());
  manager.Start();
  run_loop.Run();

  // Create the syncing status update and emit the update to the manager.
  const SyncingStatusPtr status = MakeSyncingStatus(items);
  manager.OnSyncingStatusUpdate(*status);

  // Update the item in the syncing status to have transferred 10 bytes and
  // expect the progress to return that information.
  base::RunLoop setup_progress_run_loop;
  SetState(status->item_events, ItemEvent::State::kInProgress);
  status->item_events.at(0)->bytes_transferred = 10;
  EXPECT_CALL(
      observer,
      OnProgress(AllOf(Field(&SetupProgress::pinned_bytes, 10),
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
      .WillOnce(
          DoAll(PopulateNoSearchItems(), Return(FileError::FILE_ERROR_OK)));
  EXPECT_CALL(mock_drivefs_, GetMetadata(_, _))
      .WillOnce(RunOnceCallback<1>(
          FileError::FILE_ERROR_OK,
          MakeMetadata(/*available_offline=*/true, /*size=*/128)));
  EXPECT_CALL(mock_callback, Run(SetupStage::kSuccess))
      .WillOnce(RunClosure(new_run_loop.QuitClosure()));
  SetState(status->item_events, ItemEvent::State::kCompleted);
  status->item_events.at(0)->bytes_transferred = 128;
  EXPECT_CALL(
      observer,
      OnProgress(AllOf(Field(&SetupProgress::pinned_bytes, 128),
                       Field(&SetupProgress::stage, SetupStage::kSuccess))))
      .Times(1)
      .WillOnce(RunClosure(setup_progress_run_loop.QuitClosure()));
  manager.OnSyncingStatusUpdate(*status);
  new_run_loop.Run();
}

}  // namespace drivefs::pinning
