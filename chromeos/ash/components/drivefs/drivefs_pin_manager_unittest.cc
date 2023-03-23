// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs::pinning {
namespace {

using base::BindOnce;
using base::OnceCallback;
using base::RunLoop;
using base::Seconds;
using base::SequencedTaskRunner;
using base::test::RunClosure;
using base::test::RunOnceCallback;
using base::test::TaskEnvironment;
using drive::FileError;
using mojom::FileChange;
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
using testing::UnorderedElementsAre;

using Id = PinManager::Id;
using Path = base::FilePath;
using CompletionCallback = base::MockOnceCallback<void(Stage)>;

const FileError kFileOk = FileError::FILE_ERROR_OK;

template <typename T>
std::string ToString(const T& x) {
  std::ostringstream oss;
  oss << x;
  return std::move(oss).str();
}

// Shorthand way to represent drive files with the information that is relevant
// for the pinning manager.
struct DriveItem {
  static int64_t counter;
  int64_t stable_id = ++counter;
  int64_t size = 0;
  Path path;
  FileMetadata::Type type = FileMetadata::Type::kFile;
  bool pinned = false;
  bool available_offline = false;
};

int64_t DriveItem::counter = 0;

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
    // Paths must be parented at "/root" to be considered for space
    // calculations.
    p->path = item.path.empty() ? Path("/root/file.txt") : item.path;
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

  mojom::DriveFs* GetForwardingInterface() override { NOTREACHED_NORETURN(); }

  MOCK_METHOD(void, OnStartSearchQuery, (const mojom::QueryParameters&));

  void StartSearchQuery(mojo::PendingReceiver<SearchQuery> receiver,
                        mojom::QueryParametersPtr query_params) override {
    EXPECT_FALSE(search_receiver_.is_bound());
    OnStartSearchQuery(*query_params);
    search_receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD(FileError,
              OnGetNextPage,
              (absl::optional<vector<QueryItemPtr>> * items));

  void GetNextPage(GetNextPageCallback callback) override {
    absl::optional<vector<QueryItemPtr>> items;
    const FileError error = OnGetNextPage(&items);
    SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(std::move(callback), error, std::move(items)));
  }

  MOCK_METHOD(void,
              SetPinned,
              (const Path&, bool, OnceCallback<void(FileError)>),
              (override));

  MOCK_METHOD(void,
              SetPinnedByStableId,
              (int64_t, bool, OnceCallback<void(FileError)>),
              (override));

  MOCK_METHOD(void,
              GetMetadata,
              (const Path&, OnceCallback<void(FileError, FileMetadataPtr)>),
              (override));

  MOCK_METHOD(void,
              GetMetadataByStableId,
              (int64_t, OnceCallback<void(FileError, FileMetadataPtr)>),
              (override));

 private:
  mojo::Receiver<SearchQuery> search_receiver_{this};
};

class MockSpaceGetter {
 public:
  MOCK_METHOD(void, GetFreeSpace, (const Path&, PinManager::SpaceResult));
};

class MockObserver : public PinManager::Observer {
 public:
  MOCK_METHOD(void, OnProgress, (const Progress&), (override));
  MOCK_METHOD(void, OnDrop, (), (override));
};

}  // namespace

class DriveFsPinManagerTest : public testing::Test {
 protected:
  ~DriveFsPinManagerTest() override {
    logging::SetMinLogLevel(original_log_level_);
  }

  DriveFsPinManagerTest() {
    logging::SetMinLogLevel(-3);
    CHECK(temp_dir_.CreateUniqueTempDir());
    gcache_dir_ = temp_dir_.GetPath().Append("GCache");
  }

  PinManager::SpaceGetter GetSpaceGetter() {
    return base::BindRepeating(&MockSpaceGetter::GetFreeSpace,
                               base::Unretained(&space_getter_));
  }

  const int original_log_level_ = logging::GetMinLogLevel();
  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  Path gcache_dir_;
  MockSpaceGetter space_getter_;
  testing::StrictMock<MockDriveFs> drivefs_;
};

// Tests the output operator for the Stage enum.
TEST_F(DriveFsPinManagerTest, Stage) {
  std::unordered_set<std::string> labels;
  for (const Stage stage : {
           Stage::kStopped,
           Stage::kPaused,
           Stage::kGettingFreeSpace,
           Stage::kListingFiles,
           Stage::kSyncing,
           Stage::kSuccess,
           Stage::kCannotGetFreeSpace,
           Stage::kCannotListFiles,
           Stage::kNotEnoughSpace,
           Stage(-1),
           Stage(-2),
       }) {
    const std::string label = ToString(stage);
    EXPECT_NE(label, "");
    EXPECT_TRUE(labels.insert(label).second)
        << "Not unique: " << std::quoted(label);
  }
}

// Tests PinManager::CanPin().
TEST_F(DriveFsPinManagerTest, CanPin) {
  using Type = FileMetadata::Type;
  using CanPinStatus = FileMetadata::CanPinStatus;

  Path path("/root/poi");
  FileMetadata md;
  md.stable_id = 57;
  md.size = 1456754;
  md.can_pin = CanPinStatus::kOk;
  md.pinned = false;
  md.available_offline = false;

  // Non-empty file can be pinned.
  md.type = Type::kFile;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Hosted doc can be pinned.
  md.size = 0;
  md.type = Type::kHosted;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Directory cannot be pinned.
  md.type = Type::kDirectory;
  EXPECT_FALSE(PinManager::CanPin(md, path));

  // Back to pinnable case.
  md.type = Type::kFile;
  md.size = 1;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Zero-sized file can be pinned.
  md.size = 0;
  EXPECT_TRUE(PinManager::CanPin(md, path));
  md.size = 1456754;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Unpinnable file cannot be pinned.
  md.can_pin = CanPinStatus::kDisabled;
  EXPECT_FALSE(PinManager::CanPin(md, path));
  md.can_pin = CanPinStatus::kOk;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Already pinned and cached file does not need to be pinned.
  md.pinned = true;
  md.available_offline = true;
  EXPECT_FALSE(PinManager::CanPin(md, path));

  // Already pinned file that is not cached yet should be followed as if it was
  // just pinned.
  md.pinned = true;
  md.available_offline = false;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Unpinned file should be pinned even if it is already cached.
  md.pinned = false;
  md.available_offline = true;
  EXPECT_TRUE(PinManager::CanPin(md, path));
  md.available_offline = false;
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // Shortcut cannot be pinned.
  md.shortcut_details = mojom::ShortcutDetails::New();
  md.shortcut_details->target_stable_id = 987;
  md.shortcut_details->target_lookup_status =
      mojom::ShortcutDetails::LookupStatus::kOk;
  EXPECT_FALSE(PinManager::CanPin(md, path));
  md.shortcut_details.reset();
  EXPECT_TRUE(PinManager::CanPin(md, path));

  // File that is not under /root/... cannot be pinned.
  path = Path("/shared/poi");
  EXPECT_FALSE(PinManager::CanPin(md, path));
}

// Tests PinManager::Add().
TEST_F(DriveFsPinManagerTest, Add) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 0);
    EXPECT_EQ(progress.required_space, 0);
    EXPECT_EQ(progress.skipped_items, 0);
  }

  const Id id1 = Id(101);
  const Path path1 = Path("/root/Path 1");
  const int64_t size1 = 698248964;

  const Id id2 = Id(102);
  const Path path2 = Path("/root/Path 2");
  const int64_t size2 = 78964533;

  const Id id3 = Id(103);
  const Path path3 = Path("/root/Path 3");
  const int64_t size3 = 896545;

  const Id id4 = Id(104);
  const Path path4 = Path("/root/Path 4");
  const int64_t size4 = 8645;

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Add an item.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id1);
    md.type = FileMetadata::Type::kFile;
    md.size = size1;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = false;
    EXPECT_TRUE(manager.Add(md, path1));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Try to add a conflicting item with the same ID, but different path and
  // size.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id1);
    md.type = FileMetadata::Type::kFile;
    md.size = size2;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = false;
    EXPECT_FALSE(manager.Add(md, path2));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size1);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_FALSE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, size1);
    EXPECT_EQ(progress.required_space, 698249216);
    EXPECT_EQ(progress.syncing_files, 0);
    EXPECT_EQ(progress.files_to_pin, 1);
    EXPECT_EQ(progress.skipped_items, 0);
  }

  // Add a second item, but which is already pinned this time.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id2);
    md.type = FileMetadata::Type::kFile;
    md.size = size2;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = true;
    md.available_offline = false;
    EXPECT_TRUE(manager.Add(md, path2));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const auto it = manager.files_to_track_.find(id2);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id2);
    EXPECT_EQ(file.path, path2);
    EXPECT_EQ(file.total, size2);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.in_progress);
    EXPECT_TRUE(file.pinned);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, size1 + size2);
    EXPECT_EQ(progress.required_space, 777216000);
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.files_to_pin, 2);
    EXPECT_EQ(progress.skipped_items, 0);
  }

  // Add a third item, but which is not pinned yet, although already available
  // offline.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id3);
    md.type = FileMetadata::Type::kFile;
    md.size = size3;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = true;
    EXPECT_TRUE(manager.Add(md, path3));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1, id3));
  EXPECT_THAT(manager.files_to_track_, SizeIs(3));

  {
    const auto it = manager.files_to_track_.find(id3);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id3);
    EXPECT_EQ(file.path, path3);
    EXPECT_EQ(file.total, size3);
    EXPECT_EQ(file.transferred, size3);
    EXPECT_TRUE(file.in_progress);
    EXPECT_FALSE(file.pinned);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, size3);
    EXPECT_EQ(progress.bytes_to_pin, size1 + size2 + size3);
    EXPECT_EQ(progress.required_space, 777216000);
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.files_to_pin, 3);
    EXPECT_EQ(progress.skipped_items, 0);
  }

  // Try to add a forth item, but which is both pinned and already available
  // offline. This should be skipped.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id4);
    md.type = FileMetadata::Type::kFile;
    md.size = size4;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = true;
    md.available_offline = true;
    EXPECT_FALSE(manager.Add(md, path4));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1, id3));
  EXPECT_THAT(manager.files_to_track_, SizeIs(3));

  {
    const auto it = manager.files_to_track_.find(id3);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id3);
    EXPECT_EQ(file.path, path3);
    EXPECT_EQ(file.total, size3);
    EXPECT_EQ(file.transferred, size3);
    EXPECT_TRUE(file.in_progress);
    EXPECT_FALSE(file.pinned);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, size3);
    EXPECT_EQ(progress.bytes_to_pin, size1 + size2 + size3);
    EXPECT_EQ(progress.required_space, 777216000);
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.files_to_pin, 3);
    EXPECT_EQ(progress.skipped_items, 1);
  }
}

// Tests PinManager::Update().
TEST_F(DriveFsPinManagerTest, Update) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.pinned_bytes = 5000;
  manager.progress_.bytes_to_pin = 10000;
  manager.progress_.required_space = 20480;

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  const Id id1 = Id(549);
  const Path path1 = Path("Path 1");
  const int64_t size1 = 2000;

  const Id id2 = Id(17);
  const Path path2 = Path("Path 2");
  const int64_t size2 = 5000;

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, PinManager::File{.path = path1, .total = size1});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Try to update an unknown file.
  EXPECT_FALSE(manager.Update(id2, path2, size2, size2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size1);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_FALSE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
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
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size1);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_FALSE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
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
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size2);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
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
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size2);
    EXPECT_EQ(file.transferred, size1);
    EXPECT_TRUE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 7000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // Update path.
  EXPECT_TRUE(manager.Update(id1, path2, -1, -1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path2);
    EXPECT_EQ(file.total, size2);
    EXPECT_EQ(file.transferred, size1);
    EXPECT_TRUE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 7000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  // Progress goes backwards.
  EXPECT_TRUE(manager.Update(id1, path2, 1000, -1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path2);
    EXPECT_EQ(file.total, size2);
    EXPECT_EQ(file.transferred, 1000);
    EXPECT_TRUE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 6000);
    EXPECT_EQ(progress.bytes_to_pin, 13000);
    EXPECT_EQ(progress.required_space, 20480);
  }
}

// Tests PinManager::Remove().
TEST_F(DriveFsPinManagerTest, Remove) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.pinned_bytes = 5000;
  manager.progress_.bytes_to_pin = 10000;
  manager.progress_.required_space = 20480;

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  const Id id1 = Id(549);
  const Path path1 = Path("Path 1");

  const Id id2 = Id(17);
  const Path path2 = Path("Path 2");

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, PinManager::File{.path = path1,
                              .transferred = 1200,
                              .total = 3000,
                              .pinned = true,
                              .in_progress = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.syncing_files, 1);

  // Try to remove an unknown file.
  EXPECT_FALSE(manager.Remove(id2, path2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.syncing_files, 1);

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, 3000);
    EXPECT_EQ(file.transferred, 1200);
    EXPECT_TRUE(file.in_progress);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
    EXPECT_EQ(progress.syncing_files, 1);
  }

  // Remove file with default final size.
  EXPECT_TRUE(manager.Remove(id1, path2));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 6800);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 20480);
    EXPECT_EQ(progress.syncing_files, 0);
  }

  // Put in place a file to track.
  {
    ASSERT_TRUE(manager.files_to_track_
                    .try_emplace(id1, PinManager::File{.path = path1,
                                                       .transferred = 1200,
                                                       .total = 3000,
                                                       .pinned = false,
                                                       .in_progress = true})
                    .second);
    ASSERT_TRUE(manager.files_to_pin_.insert(id1).second);
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Remove file while setting size to zero.
  EXPECT_TRUE(manager.Remove(id1, path2, 0));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5600);
    EXPECT_EQ(progress.bytes_to_pin, 7000);
    EXPECT_EQ(progress.required_space, 20480);
    EXPECT_EQ(progress.syncing_files, 0);
  }

  // Put in place a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, PinManager::File{.path = path1,
                              .transferred = 5000,
                              .total = 6000,
                              .pinned = true,
                              .in_progress = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Remove file while setting size to a different value that the expected one.
  EXPECT_TRUE(manager.Remove(id1, path1, 10000));
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 10600);
    EXPECT_EQ(progress.bytes_to_pin, 11000);
    EXPECT_EQ(progress.required_space, 20480);
    EXPECT_EQ(progress.syncing_files, 0);
  }
}

// Tests PinManager::OnFileCreated().
TEST_F(DriveFsPinManagerTest, OnFileCreated) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  const DriveItem item{.size = 2487};
  FileChange event;
  event.type = FileChange::Type::kCreate;
  event.stable_id = item.stable_id;
  event.path = Path("/root/Path 1");

  // Should not have any effect since the Pin manager is in kStopped stage.
  EXPECT_CALL(drivefs_, GetMetadataByStableId(_, _)).Times(0);
  manager.OnFileCreated(std::as_const(event));

  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 0);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  // Switch to kListingFiles stage.
  manager.progress_.stage = Stage::kListingFiles;
  EXPECT_CALL(drivefs_, GetMetadataByStableId(item.stable_id, _))
      .Times(1)
      .WillOnce(RunOnceCallback<1>(kFileOk, MakeMetadata(item)));
  manager.OnFileCreated(std::as_const(event));

  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(Id(item.stable_id)));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Calling OnFileCreated again with an already tracked ID should not have any
  // effect.
  EXPECT_CALL(drivefs_, GetMetadataByStableId(_, _)).Times(0);
  event.path = Path("/root/Path 2");
  manager.OnFileCreated(std::as_const(event));

  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(Id(item.stable_id)));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Spurious events with no stable_id should be ignored (b/268419828).
  event.stable_id = 0;
  manager.OnFileCreated(std::as_const(event));

  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(Id(item.stable_id)));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  manager.progress_.stage = Stage::kStopped;
}

// Tests PinManager::OnFileDeleted().
TEST_F(DriveFsPinManagerTest, OnFileDeleted) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  const DriveItem item{.size = 2487};
  const Path path("/root/Path 1");

  FileChange event;
  event.type = FileChange::Type::kDelete;
  event.stable_id = item.stable_id;
  event.path = path;

  EXPECT_CALL(drivefs_, SetPinnedByStableId(item.stable_id, false, _))
      .WillOnce(RunOnceCallback<2>(kFileOk));

  manager.OnFileDeleted(std::as_const(event));

  EXPECT_CALL(drivefs_, SetPinnedByStableId(item.stable_id, false, _))
      .WillOnce(RunOnceCallback<2>(FileError::FILE_ERROR_ACCESS_DENIED));

  manager.OnFileDeleted(std::as_const(event));
}

// Tests PinManager::OnFilesChanged().
TEST_F(DriveFsPinManagerTest, OnFilesChanged) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;

  int64_t id = 101;
  const Path path("/root/Path 1");

  std::vector<FileChange> events;
  {
    FileChange& event = events.emplace_back();
    event.type = FileChange::Type(-1);
    event.stable_id = id;
    event.path = path;
  }
  {
    FileChange& event = events.emplace_back();
    event.type = FileChange::Type::kCreate;
    event.stable_id = id;
    event.path = path;
  }
  {
    FileChange& event = events.emplace_back();
    event.type = FileChange::Type::kModify;
    event.stable_id = id;
    event.path = path;
  }
  {
    FileChange& event = events.emplace_back();
    event.type = FileChange::Type::kDelete;
    event.stable_id = id;
    event.path = path;
  }

  EXPECT_CALL(drivefs_, GetMetadataByStableId(id, _));
  EXPECT_CALL(drivefs_, SetPinnedByStableId(id, false, _));
  manager.OnFilesChanged(std::as_const(events));

  manager.Stop();
}

// Tests PinManager::OnFilePinned().
TEST_F(DriveFsPinManagerTest, OnFilePinned) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;

  const Id id = Id(61);
  const Path path("/root/Path 1");
  const int64_t size = 2000;

  // Cannot pin an unknown file.
  manager.OnFilePinned(id, path, FileError::FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);
  EXPECT_EQ(manager.progress_.failed_files, 0);

  // Add a file to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id, PinManager::File{.path = path, .total = size, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
    manager.progress_.bytes_to_pin += size;
    manager.progress_.required_space += 4096;
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Cannot pin a known file.
  manager.OnFilePinned(id, path, FileError::FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);
  EXPECT_EQ(manager.progress_.failed_files, 1);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  manager.progress_.failed_files = 0;

  // Pinned an unknown file.
  manager.OnFilePinned(id, path, FileError::FILE_ERROR_OK);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Add a file to track.
  const auto [it, ok] = manager.files_to_track_.try_emplace(
      id, PinManager::File{.path = path, .total = size, .pinned = true});
  ASSERT_TRUE(ok);
  manager.progress_.syncing_files++;
  manager.progress_.bytes_to_pin += size;
  manager.progress_.required_space += 4096;

  // Pinned a known file.
  manager.OnFilePinned(id, path, FileError::FILE_ERROR_OK);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2000);
  EXPECT_EQ(manager.progress_.syncing_files, 1);
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Pinned a known file that hasn't been asked to be pinned.
  it->second.pinned = false;
  manager.files_to_pin_.insert(id);
  manager.OnFilePinned(id, path, FileError::FILE_ERROR_OK);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2000);
  EXPECT_EQ(manager.progress_.syncing_files, 1);
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());

  manager.Stop();
}

// Tests PinManager::OnMetadataForCreatedFile().
TEST_F(DriveFsPinManagerTest, OnMetadataForCreatedFile) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  manager.progress_.stage = Stage::kListingFiles;

  const Id id = Id(101);
  const Path path("/root/Path 1");
  const DriveItem item{.stable_id = static_cast<int64_t>(id), .size = 2487};

  // Cannot get metadata for an untracked file.
  manager.OnMetadataForCreatedFile(id, path, drive::FILE_ERROR_ACCESS_DENIED,
                                   nullptr);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Add a tracked file.
  ASSERT_TRUE(manager.Add(*MakeMetadata(item), path));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Cannot get metadata for a tracked file.
  manager.OnMetadataForCreatedFile(
      id, path, FileError::FILE_ERROR_ACCESS_DENIED, nullptr);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 1);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Get metadata for an untracked file.
  manager.progress_.failed_files = 0;
  manager.OnMetadataForCreatedFile(id, path, kFileOk, MakeMetadata(item));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  manager.progress_.stage = Stage::kStopped;
}

// Tests PinManager::OnFileModified().
TEST_F(DriveFsPinManagerTest, OnFileModified) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  const DriveItem item{.size = 2487};
  const Id id = Id(item.stable_id);
  const Path path1("/root/Path 1");
  FileChange event;
  event.type = FileChange::Type::kModify;
  event.stable_id = item.stable_id;
  event.path = path1;

  // Should not have any effect since this file is not tracked.
  EXPECT_CALL(drivefs_, GetMetadataByStableId(_, _)).Times(0);
  manager.OnFileModified(std::as_const(event));

  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 0);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  // Add a tracked file.
  const Path path2("/root/Path 2");
  ASSERT_TRUE(manager.Add(*MakeMetadata(item), path2));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  {
    const auto it = manager.files_to_track_.find(id);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, file] = *it;
    EXPECT_EQ(got_id, id);
    EXPECT_EQ(file.path, path2);
  }

  // Should modify the path.
  EXPECT_CALL(drivefs_, GetMetadataByStableId(event.stable_id, _))
      .Times(1)
      .WillOnce(RunOnceCallback<1>(kFileOk, MakeMetadata(item)));
  manager.OnFileModified(std::as_const(event));

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  {
    const auto it = manager.files_to_track_.find(id);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, file] = *it;
    EXPECT_EQ(got_id, id);
    EXPECT_EQ(file.path, path1);
  }
}

// Tests PinManager::OnMetadataForModifiedFile().
TEST_F(DriveFsPinManagerTest, OnMetadataForModifiedFile) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  manager.progress_.stage = Stage::kListingFiles;

  const Id id = Id(101);
  const Path path("/root/Path 1");
  DriveItem item{.stable_id = static_cast<int64_t>(id), .size = 2487};

  // Cannot get metadata for an untracked file.
  manager.OnMetadataForModifiedFile(id, path, drive::FILE_ERROR_ACCESS_DENIED,
                                    nullptr);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Add a tracked and unpinned file.
  ASSERT_TRUE(manager.Add(*MakeMetadata(item), path));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Cannot get metadata for a tracked file.
  manager.OnMetadataForModifiedFile(
      id, path, FileError::FILE_ERROR_ACCESS_DENIED, nullptr);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 1);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Get metadata for an untracked file.
  manager.progress_.failed_files = 0;
  manager.OnMetadataForModifiedFile(id, path, kFileOk, MakeMetadata(item));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  // Add a tracked file.
  ASSERT_TRUE(manager.Add(*MakeMetadata(item), path));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  {
    const auto it = manager.files_to_track_.find(id);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, file] = *it;
    EXPECT_EQ(got_id, id);
    EXPECT_EQ(file.path, path);
  }

  // Metadata indicates that the file is still not pinned.
  manager.OnMetadataForModifiedFile(id, path, kFileOk, MakeMetadata(item));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  {
    const auto it = manager.files_to_track_.find(id);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [got_id, file] = *it;
    EXPECT_EQ(got_id, id);
    EXPECT_EQ(file.path, path);
    EXPECT_FALSE(file.pinned);
  }

  // Metadata indicates that the file is pinned but not available offline.
  item.pinned = true;
  manager.OnMetadataForModifiedFile(id, path, kFileOk, MakeMetadata(item));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 2487);
  EXPECT_EQ(manager.progress_.required_space, 4096);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  // Metadata indicates that the file is pinned and available offline.
  item.available_offline = true;
  item.size = 87489;
  manager.OnMetadataForModifiedFile(id, path, kFileOk, MakeMetadata(item));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 1);
  EXPECT_EQ(manager.progress_.pinned_bytes, 87489);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 87489);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  // Reset counters.
  manager.progress_.pinned_files = 0;
  manager.progress_.pinned_bytes = 0;
  manager.progress_.bytes_to_pin = 0;

  // Add a tracked and pinned file.
  item.pinned = true;
  item.available_offline = false;
  ASSERT_TRUE(manager.Add(*MakeMetadata(item), path));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  // Metadata indicates that the file has been unexpectedly unpinned.
  item.pinned = false;
  manager.OnMetadataForModifiedFile(id, path, kFileOk, MakeMetadata(item));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 1);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.bytes_to_pin, 0);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  manager.progress_.stage = Stage::kStopped;
}

// Tests PinManager::OnSyncingEvent().
TEST_F(DriveFsPinManagerTest, OnSyncingEvent) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.bytes_to_pin = 30000;
  manager.progress_.required_space = 32768;

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  const Id id1 = Id(549);
  const Path path1 = Path("Path 1");

  const Id id2 = Id(17);
  const Path path2 = Path("Path 2");

  // Put in place a couple of files to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, PinManager::File{.path = path1, .total = 10000, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id2, PinManager::File{.path = path2, .total = 20000, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  // An event with an unknown type is ignored.
  {
    ItemEvent event;
    event.is_download = true;
    event.stable_id = static_cast<int64_t>(id2);
    event.path = path2.value();
    event.state = ItemEvent::State(-1);
    event.bytes_to_transfer = -1;
    event.bytes_transferred = -1;
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 2);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  // Mark file 1 as queued.
  {
    ItemEvent event;
    event.is_download = true;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1.value();
    event.state = ItemEvent::State::kQueued;
    event.bytes_to_transfer = 0;
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 2);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, 10000);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.pinned);
    EXPECT_FALSE(file.in_progress);
  }

  // Mark file 1 as in progress.
  {
    ItemEvent event;
    event.is_download = true;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1.value();
    event.state = ItemEvent::State::kInProgress;
    event.bytes_to_transfer = 10000;
    event.bytes_transferred = 5000;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  // Upload events should be ignored.
  {
    ItemEvent event;
    event.is_download = false;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1.value();
    event.state = ItemEvent::State::kInProgress;
    event.bytes_to_transfer = 30000;
    event.bytes_transferred = 7000;
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 2);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 5000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 24576);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, 10000);
    EXPECT_EQ(file.transferred, 5000);
    EXPECT_TRUE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  // Mark file 1 as completed.
  {
    ItemEvent event;
    event.is_download = true;
    event.stable_id = static_cast<int64_t>(id1);
    event.path = path1.value();
    event.state = ItemEvent::State::kCompleted;
    event.bytes_to_transfer = -1;
    event.bytes_transferred = -1;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 10000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 20480);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    EXPECT_EQ(it, manager.files_to_track_.end());
  }

  // Mark file 2 as failed.
  {
    ItemEvent event;
    event.is_download = true;
    event.stable_id = static_cast<int64_t>(id2);
    event.path = path2.value();
    event.state = ItemEvent::State::kFailed;
    event.bytes_to_transfer = -1;
    event.bytes_transferred = -1;
    EXPECT_TRUE(manager.OnSyncingEvent(event));
    EXPECT_FALSE(manager.OnSyncingEvent(event));
  }

  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 0);
    EXPECT_EQ(progress.failed_files, 1);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 10000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 0);
  }

  {
    const auto it = manager.files_to_track_.find(id2);
    EXPECT_EQ(it, manager.files_to_track_.end());
  }
}

// Tests PinManager::OnSyncingStatusUpdate().
TEST_F(DriveFsPinManagerTest, OnSyncingStatusUpdate) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  manager.progress_.bytes_to_pin = 30000;
  manager.progress_.required_space = 32768;

  const Id id1 = Id(549);
  const Path path1 = Path("Path 1");

  const Id id2 = Id(17);
  const Path path2 = Path("Path 2");

  // Put in place a couple of files to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, PinManager::File{.path = path1, .total = 10000, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id2, PinManager::File{.path = path2, .total = 20000, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  // Prepare a list of syncing status events.
  SyncingStatus events;

  {
    // An event with an unknown type is ignored.
    ItemEventPtr event = ItemEvent::New();
    event->is_download = true;
    event->stable_id = static_cast<int64_t>(id2);
    event->path = path2.value();
    event->state = ItemEvent::State(-1);
    event->bytes_to_transfer = -1;
    event->bytes_transferred = -1;
    events.item_events.push_back(std::move(event));
  }
  {
    // Mark file 1 as queued.
    ItemEventPtr event = ItemEvent::New();
    event->is_download = true;
    event->stable_id = static_cast<int64_t>(id1);
    event->path = path1.value();
    event->state = ItemEvent::State::kQueued;
    event->bytes_to_transfer = 0;
    events.item_events.push_back(ItemEvent::New(*event));
    events.item_events.push_back(std::move(event));
  }
  {
    // Mark file 1 as in progress.
    ItemEventPtr event = ItemEvent::New();
    event->is_download = true;
    event->stable_id = static_cast<int64_t>(id1);
    event->path = path1.value();
    event->state = ItemEvent::State::kInProgress;
    event->bytes_to_transfer = 10000;
    event->bytes_transferred = 5000;
    events.item_events.push_back(ItemEvent::New(*event));
    events.item_events.push_back(std::move(event));
  }
  {
    // Upload events should be ignored.
    ItemEventPtr event = ItemEvent::New();
    event->is_download = false;
    event->stable_id = static_cast<int64_t>(id1);
    event->path = path1.value();
    event->state = ItemEvent::State::kInProgress;
    event->bytes_to_transfer = 30000;
    event->bytes_transferred = 7000;
    events.item_events.push_back(ItemEvent::New(*event));
    events.item_events.push_back(std::move(event));
  }
  {
    // Mark file 1 as completed.
    ItemEventPtr event = ItemEvent::New();
    event->is_download = true;
    event->stable_id = static_cast<int64_t>(id1);
    event->path = path1.value();
    event->state = ItemEvent::State::kCompleted;
    event->bytes_to_transfer = -1;
    event->bytes_transferred = -1;
    events.item_events.push_back(ItemEvent::New(*event));
    events.item_events.push_back(std::move(event));
  }
  {
    // Mark file 2 as failed.
    ItemEventPtr event = ItemEvent::New();
    event->is_download = true;
    event->stable_id = static_cast<int64_t>(id2);
    event->path = path2.value();
    event->state = ItemEvent::State::kFailed;
    event->bytes_to_transfer = -1;
    event->bytes_transferred = -1;
    events.item_events.push_back(ItemEvent::New(*event));
    events.item_events.push_back(std::move(event));
  }

  manager.OnSyncingStatusUpdate(std::as_const(events));

  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
    EXPECT_EQ(progress.syncing_files, 0);
    EXPECT_EQ(progress.failed_files, 1);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 10000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 0);
    EXPECT_EQ(progress.useful_events, 3);
    EXPECT_EQ(progress.duplicated_events, 8);
  }

  manager.Stop();

  // Events received when the PinManager is stopped are ignored.
  manager.OnSyncingStatusUpdate(std::as_const(events));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
    EXPECT_EQ(progress.syncing_files, 0);
    EXPECT_EQ(progress.failed_files, 1);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 10000);
    EXPECT_EQ(progress.bytes_to_pin, 10000);
    EXPECT_EQ(progress.required_space, 0);
    EXPECT_EQ(progress.useful_events, 3);
    EXPECT_EQ(progress.duplicated_events, 8);
  }
}

// Tests what happens when PinManager cannot get free space during initial
// setup.
TEST_F(DriveFsPinManagerTest, CannotGetFreeSpace1) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(0);
  EXPECT_CALL(drivefs_, OnGetNextPage(_)).Times(0);
  EXPECT_CALL(completion_callback, Run(Stage::kCannotGetFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  manager.Start();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kCannotGetFreeSpace);
  EXPECT_EQ(progress.free_space, 0);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests what happens when PinManager cannot get free space during the periodic
// check.
TEST_F(DriveFsPinManagerTest, CannotGetFreeSpace2) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(completion_callback, Run(Stage::kCannotGetFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  manager.CheckFreeSpace();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kCannotGetFreeSpace);
  EXPECT_EQ(progress.free_space, 0);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, CannotListFiles) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_FAILED));
  EXPECT_CALL(completion_callback, Run(Stage::kCannotListFiles))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  manager.Start();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kCannotListFiles);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, InvalidFileList) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_)).WillOnce(Return(kFileOk));
  EXPECT_CALL(completion_callback, Run(Stage::kCannotListFiles))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  manager.Start();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kCannotListFiles);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests what happens when PinManager cannot get enough free space during
// the initial setup.
TEST_F(DriveFsPinManagerTest, NotEnoughSpace) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  // Mock Drive search to return 3 unpinned files that total just above 512 MB.
  // The available space of 1 GB is not enough if you take in account the 512 MB
  // margin.
  const vector<DriveItem> items = {
      {.size = 300 << 20}, {.size = 212 << 20}, {.size = 1}};

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items), Return(kFileOk)))
      .WillOnce(DoAll(PopulateNoSearchItems(), Return(kFileOk)));
  EXPECT_CALL(completion_callback, Run(Stage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  manager.Start();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, (512 << 20) + (4 << 10));
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests what happens when PinManager cannot get enough free space during
// the periodic check.
TEST_F(DriveFsPinManagerTest, NotEnoughSpace2) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(completion_callback, Run(Stage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(200 << 20));  // 200 MB

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  manager.CheckFreeSpace();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, 200 << 20);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests what happens when there is enough free space during the periodic check.
TEST_F(DriveFsPinManagerTest, OnFreeSpaceRetrieved2) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  manager.OnFreeSpaceRetrieved2(1 << 30);

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kSyncing);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);

  manager.progress_.stage = Stage::kStopped;
}

// Tests that the space check is actually periodic.
TEST_F(DriveFsPinManagerTest, PeriodicSpaceCheck) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(completion_callback, Run(Stage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30))     // 1 GB is enough space
      .WillOnce(RunOnceCallback<1>(800 << 20))   // 800 MB is enough space
      .WillOnce(RunOnceCallback<1>(600 << 20))   // 600 MB is enough space
      .WillOnce(RunOnceCallback<1>(400 << 20));  // 400 MB is not enough

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);

  // Check the original time interval.
  EXPECT_EQ(manager.space_check_interval_, base::Seconds(60));

  // But use a much shorter interval for this test.
  manager.space_check_interval_ = base::Milliseconds(100);

  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  manager.progress_.stage = Stage::kSyncing;

  manager.CheckFreeSpace();

  // There should be 3 iterations of 100 ms each.
  base::ElapsedTimer timer;
  run_loop.Run();
  EXPECT_GE(timer.Elapsed(), base::Milliseconds(300));

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, 400 << 20);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinManagerTest, JustCheckRequiredSpace) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  // Mock Drive search to return 2 unpinned files that total to 512 MB. The
  // available space of 1 GB is just enough if you take in account the 512 MB
  // margin.
  const vector<DriveItem> items = {{.size = 300 << 20}, {.size = 212 << 20}};

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items), Return(kFileOk)))
      .WillOnce(DoAll(PopulateNoSearchItems(), Return(kFileOk)));
  EXPECT_CALL(completion_callback, Run(Stage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.ShouldPin(false);
  manager.SetCompletionCallback(completion_callback.Get());
  manager.Start();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kSuccess);
  EXPECT_EQ(progress.free_space, 1 << 30);
  EXPECT_EQ(progress.required_space, 512 << 20);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests PinManager::SetOnline().
TEST_F(DriveFsPinManagerTest, SetOnline) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.SetSpaceGetter(GetSpaceGetter());

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  EXPECT_TRUE(manager.is_online_);

  manager.SetOnline(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  EXPECT_FALSE(manager.is_online_);

  manager.SetOnline(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  EXPECT_TRUE(manager.is_online_);

  manager.SetOnline(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  EXPECT_FALSE(manager.is_online_);

  manager.Start();
  EXPECT_EQ(manager.progress_.stage, Stage::kPaused);
  EXPECT_FALSE(manager.is_online_);

  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  manager.SetOnline(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);
  EXPECT_TRUE(manager.is_online_);

  manager.SetOnline(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPaused);
  EXPECT_FALSE(manager.is_online_);

  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  manager.SetOnline(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);
  EXPECT_TRUE(manager.is_online_);

  manager.SetOnline(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPaused);
  EXPECT_FALSE(manager.is_online_);

  manager.Stop();
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  EXPECT_FALSE(manager.is_online_);
}

// Tests PinManager::OnSearchResult() with transient errors.
TEST_F(DriveFsPinManagerTest, OnTransientError) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  manager.StartSearchQuery();

  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_NO_CONNECTION));
  manager.GetNextPage();
  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);

  task_environment_.FastForwardBy(Seconds(4));
  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_SERVICE_UNAVAILABLE));
  task_environment_.FastForwardBy(Seconds(1));
  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);

  task_environment_.FastForwardBy(Seconds(4));
  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_NO_MEMORY));
  task_environment_.FastForwardBy(Seconds(1));

  EXPECT_EQ(manager.progress_.stage, Stage::kCannotListFiles);
}

// Tests PinManager::OnError().
TEST_F(DriveFsPinManagerTest, OnError) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;

  using mojom::DriveError;
  DriveError error;
  error.stable_id = 214;
  error.path = Path("My Path");

  using Type = DriveError::Type;

  // These error types should just be logged.
  for (const Type type : {
           Type::kCantUploadStorageFull,
           Type::kCantUploadStorageFullOrganization,
           Type::kCantUploadSharedDriveStorageFull,
           Type(-1),

       }) {
    error.type = type;
    manager.OnError(std::as_const(error));
    EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  }

  // Error of type kPinningFailedDiskFull should stop the pin manager.
  error.type = Type::kPinningFailedDiskFull;
  manager.OnError(std::as_const(error));
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);

  manager.Stop();
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  // Error of type kPinningFailedDiskFull should not have any effect if the pin
  // manager is already stopped.
  manager.OnError(std::as_const(error));
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
}

// Tests that calling PinManager::Start() when the PinManager is already in
// progress does not have any effect.
TEST_F(DriveFsPinManagerTest, StartWhenInProgress) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kGettingFreeSpace;
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.Start();
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.Stop();
}

// Tests PinManager::StartPinning().
TEST_F(DriveFsPinManagerTest, StartPinning) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;
  DCHECK_EQ(manager.progress_.free_space, 0);

  manager.StartPinning();
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);

  manager.progress_.stage = Stage::kListingFiles;
  manager.progress_.free_space = 1 << 30;  // 1 GB

  EXPECT_TRUE(manager.should_pin_);
  manager.ShouldPin(false);
  EXPECT_FALSE(manager.should_pin_);

  manager.StartPinning();
  EXPECT_EQ(manager.progress_.stage, Stage::kSuccess);

  manager.progress_.stage = Stage::kListingFiles;
  manager.ShouldPin(true);
  EXPECT_TRUE(manager.should_pin_);

  const Id id1 = Id(101);
  const Path path1 = Path("/root/Path 1");
  const int64_t size1 = 6248964;

  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Add an item.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id1);
    md.type = FileMetadata::Type::kFile;
    md.size = size1;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = false;
    EXPECT_TRUE(manager.Add(md, path1));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  manager.SetSpaceGetter(GetSpaceGetter());
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  EXPECT_CALL(drivefs_, SetPinnedByStableId(static_cast<int64_t>(id1), true, _))
      .Times(1);
  manager.StartPinning();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);

  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size1);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  task_environment_.FastForwardBy(Seconds(19));
  EXPECT_CALL(drivefs_, GetMetadataByStableId(static_cast<int64_t>(id1), _))
      .Times(1);
  task_environment_.FastForwardBy(Seconds(1));

  manager.Stop();
}

// Tests PinManager::PinSomeFiles().
TEST_F(DriveFsPinManagerTest, PinSomeFiles) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;
  DCHECK_EQ(manager.progress_.free_space, 0);

  manager.progress_.stage = Stage::kSyncing;
  manager.progress_.free_space = 1 << 30;  // 1 GB

  const Id id1 = Id(101);
  const Path path1 = Path("/root/Path 1");
  const int64_t size1 = 6248964;

  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Add an item.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id1);
    md.type = FileMetadata::Type::kFile;
    md.size = size1;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = false;
    EXPECT_TRUE(manager.Add(md, path1));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.syncing_files, 0);

  EXPECT_CALL(drivefs_, SetPinnedByStableId(static_cast<int64_t>(id1), true, _))
      .Times(1);
  manager.PinSomeFiles();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);

  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.syncing_files, 1);

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size1);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  // Add 70 items to pin.
  for (int i = 0; i < 70; ++i) {
    FileMetadata md;
    md.stable_id = 200 + i;
    md.type = FileMetadata::Type::kFile;
    md.size = 1000 + 10 * i;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = false;
    EXPECT_TRUE(
        manager.Add(md, Path(base::StringPrintf("/root/Path %02d", i))));
  }

  EXPECT_THAT(manager.files_to_pin_, SizeIs(70));
  EXPECT_THAT(manager.files_to_track_, SizeIs(71));
  EXPECT_EQ(manager.progress_.syncing_files, 1);

  EXPECT_CALL(drivefs_, SetPinnedByStableId(_, true, _)).Times(49);
  manager.PinSomeFiles();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  EXPECT_THAT(manager.files_to_pin_, SizeIs(21));
  EXPECT_THAT(manager.files_to_track_, SizeIs(71));
  EXPECT_EQ(manager.progress_.syncing_files, 50);

  // Remove 30 files from the set of files to track.
  {
    std::vector<Id> pinned_ids;
    pinned_ids.reserve(manager.files_to_track_.size());
    for (const auto& [id, file] : manager.files_to_track_) {
      if (file.pinned) {
        pinned_ids.push_back(id);
      }
    }

    EXPECT_THAT(pinned_ids, SizeIs(50));
    pinned_ids.resize(30);
    for (const Id id : pinned_ids) {
      manager.Remove(id, Path(), 0);
    }
  }

  EXPECT_THAT(manager.files_to_pin_, SizeIs(21));
  EXPECT_THAT(manager.files_to_track_, SizeIs(41));
  EXPECT_EQ(manager.progress_.syncing_files, 20);

  EXPECT_CALL(drivefs_, SetPinnedByStableId(_, true, _)).Times(21);
  manager.PinSomeFiles();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, SizeIs(41));
  EXPECT_EQ(manager.progress_.syncing_files, 41);

  manager.files_to_track_.clear();
  manager.progress_.syncing_files = 0;
  EXPECT_FALSE(manager.progress_.emptied_queue);
  manager.PinSomeFiles();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_TRUE(manager.progress_.emptied_queue);

  manager.progress_.emptied_queue = false;
  task_environment_.FastForwardBy(Seconds(4));
  manager.PinSomeFiles();
  EXPECT_TRUE(manager.progress_.emptied_queue);

  manager.progress_.emptied_queue = false;
  task_environment_.FastForwardBy(Seconds(400));
  manager.PinSomeFiles();
  EXPECT_TRUE(manager.progress_.emptied_queue);

  manager.progress_.emptied_queue = false;
  task_environment_.FastForwardBy(Seconds(4000));
  manager.PinSomeFiles();
  EXPECT_TRUE(manager.progress_.emptied_queue);

  manager.Stop();
}

// Tests PinManager::CheckStalledFiles().
TEST_F(DriveFsPinManagerTest, CheckStalledFiles) {
  PinManager manager(temp_dir_.GetPath(), &drivefs_);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  manager.progress_.free_space = 1 << 30;  // 1 GB

  const Id id1 = Id(101);
  const Path path1 = Path("/root/Path 1");
  const int64_t size1 = 6248964;

  const Id id2 = Id(102);
  const Path path2 = Path("/root/Path 2");
  const int64_t size2 = 257835;

  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());

  // Add an item.
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id1);
    md.type = FileMetadata::Type::kFile;
    md.size = size1;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = true;
    md.available_offline = false;
    EXPECT_TRUE(manager.Add(md, path1));
  }
  {
    FileMetadata md;
    md.stable_id = static_cast<int64_t>(id2);
    md.type = FileMetadata::Type::kFile;
    md.size = size2;
    md.can_pin = FileMetadata::CanPinStatus::kOk;
    md.pinned = false;
    md.available_offline = false;
    EXPECT_TRUE(manager.Add(md, path2));
  }

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, path1);
    EXPECT_EQ(file.total, size1);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  manager.CheckStalledFiles();

  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(id2));
  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_FALSE(file.in_progress);
  }

  task_environment_.FastForwardBy(Seconds(9));
  EXPECT_CALL(drivefs_, GetMetadataByStableId(static_cast<int64_t>(id1), _))
      .Times(1);
  task_environment_.FastForwardBy(Seconds(1));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_FALSE(file.in_progress);
    file.in_progress = true;
  }

  task_environment_.FastForwardBy(Seconds(19));

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_FALSE(file.in_progress);
  }

  EXPECT_CALL(drivefs_, GetMetadataByStableId(static_cast<int64_t>(id1), _))
      .Times(10);
  task_environment_.FastForwardBy(Seconds(100));

  manager.Stop();
}

// Tests that PinManager's destructor calls OnDrop on the registered observer.
TEST_F(DriveFsPinManagerTest, OnDrop) {
  {
    MockObserver observer;
    PinManager::Observer observer2;
    PinManager manager(temp_dir_.GetPath(), &drivefs_);
    manager.AddObserver(&observer);
    manager.AddObserver(&observer2);
    EXPECT_CALL(observer, OnDrop()).Times(1);
  }
  {
    MockObserver observer;
    PinManager::Observer observer2;
    EXPECT_CALL(observer, OnDrop()).Times(0);
    PinManager manager(temp_dir_.GetPath(), &drivefs_);
    manager.AddObserver(&observer);
    manager.AddObserver(&observer2);
    manager.RemoveObserver(&observer);
  }
}

// Tests PinManager::NotifyProgress.
TEST_F(DriveFsPinManagerTest, NotifyProgress) {
  MockObserver observer;
  PinManager::Observer observer2;
  PinManager manager(temp_dir_.GetPath(), &drivefs_);
  manager.AddObserver(&observer);
  manager.AddObserver(&observer2);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_CALL(observer, OnProgress(testing::Ref(manager.progress_))).Times(1);
  manager.NotifyProgress();
  manager.RemoveObserver(&observer);
}

}  // namespace drivefs::pinning
