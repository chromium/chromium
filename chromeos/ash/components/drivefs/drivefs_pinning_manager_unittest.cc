// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"

#include <iomanip>
#include <list>
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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs::pinning {
namespace {

using ash::FakeSpacedClient;
using ash::FakeUserDataAuthClient;
using ash::SpacedClient;
using ash::UserDataAuthClient;
using base::BindOnce;
using base::OnceCallback;
using base::RunLoop;
using base::Seconds;
using base::SequencedTaskRunner;
using base::test::RunClosure;
using base::test::RunOnceCallback;
using base::test::TaskEnvironment;
using drive::FileError;
using mojom::DocsOfflineEnableStatus;
using mojom::FileChange;
using mojom::FileMetadata;
using mojom::FileMetadataPtr;
using mojom::ItemEvent;
using mojom::ItemEventPtr;
using mojom::ProgressEvent;
using mojom::QueryItem;
using mojom::QueryItemPtr;
using mojom::QueryParameters;
using mojom::SearchQuery;
using mojom::SyncingStatus;
using mojom::SyncingStatusPtr;
using std::string;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Field;
using testing::Invoke;
using testing::IsEmpty;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

using Id = PinningManager::Id;
using Path = base::FilePath;
using CompletionCallback = base::MockOnceCallback<void(Stage)>;

const FileError kFileOk = FileError::FILE_ERROR_OK;

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
// returns the appropriate type (instead of `std::nullopt`).
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
    NOTREACHED() << "No calls should make it to the forwarding interface";
  }

  MOCK_METHOD(void, OnStartSearchQuery, (const QueryParameters&));

  void StartSearchQuery(mojo::PendingReceiver<SearchQuery> query,
                        const mojom::QueryParametersPtr query_params) override {
    EXPECT_TRUE(query_params);
    queries_.emplace_back(this, std::move(query));
    OnStartSearchQuery(*query_params);
  }

  MOCK_METHOD(FileError,
              OnGetNextPage,
              (std::optional<vector<QueryItemPtr>> * items));

  void GetNextPage(GetNextPageCallback callback) override {
    std::optional<vector<QueryItemPtr>> items;
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

  MOCK_METHOD(void,
              SetDocsOfflineEnabled,
              (bool, OnceCallback<void(FileError, DocsOfflineEnableStatus)>),
              (override));

 private:
  std::list<mojo::Receiver<SearchQuery>> queries_;
};

class MockSpaceGetter {
 public:
  MOCK_METHOD(void, GetFreeSpace, (const Path&, PinningManager::SpaceResult));
};

class MockObserver : public PinningManager::Observer {
 public:
  MOCK_METHOD(void, OnProgress, (const Progress&), (override));
};

constexpr int kMaxQueueSize = 200;

}  // namespace

class DriveFsPinningManagerTest : public testing::Test {
 protected:
  ~DriveFsPinningManagerTest() override {
    logging::SetMinLogLevel(original_log_level_);
  }

  DriveFsPinningManagerTest() {
    logging::SetMinLogLevel(-3);
    CHECK(temp_dir_.CreateUniqueTempDir());
    profile_path_ = temp_dir_.GetPath().Append("Profile");
    gcache_dir_ = profile_path_.Append("GCache");
    mount_path_ = temp_dir_.GetPath();
  }

  void SetUp() override {
    UserDataAuthClient::InitializeFake();
    SpacedClient::InitializeFake();
    chromeos::PowerManagerClient::InitializeFake();
  }

  void TearDown() override {
    UserDataAuthClient::Shutdown();
    SpacedClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  PinningManager::SpaceGetter GetSpaceGetter() {
    return base::BindRepeating(&MockSpaceGetter::GetFreeSpace,
                               base::Unretained(&space_getter_));
  }

  const int original_log_level_ = logging::GetMinLogLevel();
  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath profile_path_;
  base::FilePath mount_path_;
  Path gcache_dir_;
  MockSpaceGetter space_getter_;
  testing::StrictMock<MockDriveFs> drivefs_;
};

// Tests ToString(Stage).
TEST_F(DriveFsPinningManagerTest, Stage) {
  std::unordered_set<std::string> labels;
  for (const Stage stage : {
           Stage::kStopped,
           Stage::kPausedOffline,
           Stage::kPausedBatterySaver,
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

// Tests Progress::IsError().
TEST_F(DriveFsPinningManagerTest, IsError) {
  for (const Stage stage : {
           Stage::kCannotGetFreeSpace,
           Stage::kCannotListFiles,
           Stage::kNotEnoughSpace,
       }) {
    Progress progress;
    progress.stage = stage;
    EXPECT_TRUE(std::as_const(progress).IsError()) << " for " << stage;
  }

  for (const Stage stage : {
           Stage::kStopped,
           Stage::kPausedOffline,
           Stage::kPausedBatterySaver,
           Stage::kGettingFreeSpace,
           Stage::kListingFiles,
           Stage::kSyncing,
           Stage::kSuccess,
       }) {
    Progress progress;
    progress.stage = stage;
    EXPECT_FALSE(std::as_const(progress).IsError()) << " for " << stage;
  }
}

// Tests PinningManager::CanPin().
TEST_F(DriveFsPinningManagerTest, CanPin) {
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
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Hosted doc can't be pinned.
  md.size = 0;
  md.type = Type::kHosted;
  EXPECT_FALSE(PinningManager::CanPin(md, path));

  // Directory cannot be pinned.
  md.type = Type::kDirectory;
  EXPECT_FALSE(PinningManager::CanPin(md, path));

  // Back to pinnable case.
  md.type = Type::kFile;
  md.size = 1;
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Zero-sized file can be pinned.
  md.size = 0;
  EXPECT_TRUE(PinningManager::CanPin(md, path));
  md.size = 1456754;
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Unpinnable file cannot be pinned.
  md.can_pin = CanPinStatus::kDisabled;
  EXPECT_FALSE(PinningManager::CanPin(md, path));
  md.can_pin = CanPinStatus::kOk;
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Already pinned and cached file does not need to be pinned.
  md.pinned = true;
  md.available_offline = true;
  EXPECT_FALSE(PinningManager::CanPin(md, path));

  // Already pinned file that is not cached yet should be monitored as if it was
  // just pinned.
  md.pinned = true;
  md.available_offline = false;
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Unpinned file should be pinned even if it is already cached.
  md.pinned = false;
  md.available_offline = true;
  EXPECT_TRUE(PinningManager::CanPin(md, path));
  md.available_offline = false;
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Trashed item shouldn't be pinned.
  md.trashed = true;
  EXPECT_FALSE(PinningManager::CanPin(md, path));
  md.trashed = false;
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // Shortcut cannot be pinned.
  md.shortcut_details = mojom::ShortcutDetails::New();
  md.shortcut_details->target_stable_id = 987;
  md.shortcut_details->target_lookup_status =
      mojom::ShortcutDetails::LookupStatus::kOk;
  EXPECT_FALSE(PinningManager::CanPin(md, path));
  md.shortcut_details.reset();
  EXPECT_TRUE(PinningManager::CanPin(md, path));

  // File that is not under /root/... can be pinned.
  path = Path("/shared/poi");
  EXPECT_TRUE(PinningManager::CanPin(md, path));
}

// Tests PinningManager::Add().
TEST_F(DriveFsPinningManagerTest, Add) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

  // Pretend that all the files have already been synchronised.
  EXPECT_FALSE(manager.progress_.emptied_queue);
  manager.progress_.pinned_files = 3;
  manager.progress_.pinned_bytes = size1 + size2 + size3;
  manager.progress_.files_to_pin = 0;
  manager.progress_.required_space = 0;
  manager.progress_.syncing_files = 0;
  manager.progress_.emptied_queue = true;
  manager.files_to_pin_.clear();
  manager.files_to_track_.clear();

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

  // The pinned files and bytes should have been reset.
  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, size1);
    EXPECT_EQ(progress.required_space, 698249216);
    EXPECT_EQ(progress.syncing_files, 0);
    EXPECT_EQ(progress.files_to_pin, 1);
  }
}

// Tests PinningManager::Update().
TEST_F(DriveFsPinningManagerTest, Update) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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
        id1, PinningManager::File{.path = path1, .total = size1});
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

// Tests PinningManager::Remove().
TEST_F(DriveFsPinningManagerTest, Remove) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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
        id1, PinningManager::File{.path = path1,
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
                    .try_emplace(id1, PinningManager::File{.path = path1,
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
        id1, PinningManager::File{.path = path1,
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

// Tests PinningManager::OnFileCreated().
TEST_F(DriveFsPinningManagerTest, OnFileCreated) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

  // Events with path outside /root should be ignored.
  event.path = Path("/.files-by-id/54853");
  event.stable_id++;
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

// Tests PinningManager::OnFileDeleted().
TEST_F(DriveFsPinningManagerTest, OnFileDeleted) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;

  const DriveItem item{.size = 2487};
  const Path path("/root/Path 1");

  FileChange event;
  event.type = FileChange::Type::kDelete;
  event.stable_id = item.stable_id;
  event.path = path;

  // Add a tracked file.
  ASSERT_TRUE(manager.Add(*MakeMetadata(item), path));
  EXPECT_THAT(manager.files_to_pin_, SizeIs(1));
  EXPECT_THAT(manager.files_to_track_, SizeIs(1));
  EXPECT_EQ(manager.progress_.failed_files, 0);

  // Notify that the file has been deleted.
  manager.OnFileDeleted(std::as_const(event));

  // The file should have been removed from the tracked files.
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 1);

  // Receiving a delete notification for an untracked file shouldn't do
  // anything.
  manager.OnFileDeleted(std::as_const(event));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, IsEmpty());
  EXPECT_EQ(manager.progress_.failed_files, 1);

  manager.Stop();
}

// Tests PinningManager::OnFilesChanged().
TEST_F(DriveFsPinningManagerTest, OnFilesChanged) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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
  manager.OnFilesChanged(std::as_const(events));

  manager.Stop();
}

// Tests PinningManager::OnFilePinned().
TEST_F(DriveFsPinningManagerTest, OnFilePinned) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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
        id, PinningManager::File{.path = path, .total = size, .pinned = true});
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
      id, PinningManager::File{.path = path, .total = size, .pinned = true});
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

// Tests PinningManager::OnMetadataForCreatedFile().
TEST_F(DriveFsPinningManagerTest, OnMetadataForCreatedFile) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

// Tests PinningManager::OnFileModified().
TEST_F(DriveFsPinningManagerTest, OnFileModified) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

// Tests PinningManager::OnMetadataForModifiedFile().
TEST_F(DriveFsPinningManagerTest, OnMetadataForModifiedFile) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

// Tests PinningManager::OnItemProgress().
TEST_F(DriveFsPinningManagerTest, OnItemProgress) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.bytes_to_pin = 30000;
  manager.progress_.required_space = 32768;
  manager.progress_.stage = Stage::kSyncing;

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 0);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 32768);
  }

  const Id id1 = Id(549);
  const Path path1 = mount_path_.Append("Path 1");

  const Id id2 = Id(17);
  const Path path2 = mount_path_.Append("Path 2");

  // Put in place a couple of files to track.
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id1, PinningManager::File{
                 .path = Path("/Path 1"), .total = 10000, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
  }
  {
    const auto [it, ok] = manager.files_to_track_.try_emplace(
        id2, PinningManager::File{
                 .path = Path("/Path 2"), .total = 20000, .pinned = true});
    ASSERT_TRUE(ok);
    manager.progress_.syncing_files++;
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
    ProgressEvent event;
    event.stable_id = static_cast<int64_t>(id1);
    event.file_path = path1;
    event.progress = 0;
    manager.OnItemProgress(event);
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
    EXPECT_EQ(file.path, Path("/Path 1"));
    EXPECT_EQ(file.total, 10000);
    EXPECT_EQ(file.transferred, 0);
    EXPECT_TRUE(file.pinned);
    EXPECT_FALSE(file.in_progress);
  }

  // Mark file 1 as in progress.
  {
    ProgressEvent event;
    event.stable_id = static_cast<int64_t>(id1);
    event.file_path = path1;
    event.progress = 20;
    manager.OnItemProgress(event);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 2);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 2000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 28672);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id1);
    EXPECT_EQ(file.path, Path("/Path 1"));
    EXPECT_EQ(file.total, 10000);
    EXPECT_EQ(file.transferred, 2000);
    EXPECT_TRUE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  // Mark file 2 as in progress.
  {
    ProgressEvent event;
    event.stable_id = static_cast<int64_t>(id2);
    event.file_path = path2;
    event.progress = 50;
    manager.OnItemProgress(event);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(2));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 2);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 0);
    EXPECT_EQ(progress.pinned_bytes, 12000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 16384);
  }

  {
    const auto it = manager.files_to_track_.find(id2);
    ASSERT_NE(it, manager.files_to_track_.end());
    const auto& [id, file] = *it;
    EXPECT_EQ(id, id2);
    EXPECT_EQ(file.path, Path("/Path 2"));
    EXPECT_EQ(file.total, 20000);
    EXPECT_EQ(file.transferred, 10000);
    EXPECT_TRUE(file.pinned);
    EXPECT_TRUE(file.in_progress);
  }

  // Mark file 1 as completed.
  {
    ProgressEvent event;
    event.stable_id = static_cast<int64_t>(id1);
    event.file_path = path1;
    event.progress = 100;
    manager.OnItemProgress(event);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 20000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 8192);
  }

  {
    const auto it = manager.files_to_track_.find(id1);
    EXPECT_EQ(it, manager.files_to_track_.end());
  }

  // Paths not parented at the `mount_path_` should be ignored.
  {
    ProgressEvent event;
    event.stable_id = 329;
    event.file_path = profile_path_.Append("Path 3");
    event.progress = 0;
    manager.OnItemProgress(event);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 20000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 8192);
  }

  // Untracked ids should not increment the pinned file count.
  {
    ProgressEvent event;
    event.stable_id = 458;
    event.file_path = mount_path_.Append("Path 2");
    event.progress = 100;
    manager.OnItemProgress(event);
  }

  EXPECT_THAT(manager.files_to_track_, SizeIs(1));

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(progress.syncing_files, 1);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 20000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 8192);
  }

  manager.Stop();

  // Events received when the PinningManager is stopped are ignored.
  {
    ProgressEvent event;
    event.stable_id = static_cast<int64_t>(id2);
    event.file_path = path2;
    event.progress = 80;
    manager.OnItemProgress(event);
  }

  {
    const Progress progress = manager.GetProgress();
    EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
    EXPECT_EQ(progress.syncing_files, 0);
    EXPECT_EQ(progress.failed_files, 0);
    EXPECT_EQ(progress.pinned_files, 1);
    EXPECT_EQ(progress.pinned_bytes, 20000);
    EXPECT_EQ(progress.bytes_to_pin, 30000);
    EXPECT_EQ(progress.required_space, 8192);
  }
}

// Tests what happens when PinningManager cannot get free space during initial
// setup.
TEST_F(DriveFsPinningManagerTest, CannotGetFreeSpace1) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(0);
  EXPECT_CALL(drivefs_, OnGetNextPage(_)).Times(0);
  EXPECT_CALL(completion_callback, Run(Stage::kCannotGetFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
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

// Tests what happens when PinningManager cannot get free space during the
// periodic check.
TEST_F(DriveFsPinningManagerTest, CannotGetFreeSpace2) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(completion_callback, Run(Stage::kCannotGetFreeSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(-1));

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
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

TEST_F(DriveFsPinningManagerTest, CannotListFiles) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_FAILED));
  EXPECT_CALL(completion_callback, Run(Stage::kCannotListFiles))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(1 << 30));  // 1 GB.

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
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

// Tests what happens when PinningManager cannot get enough free space during
// the initial setup.
TEST_F(DriveFsPinningManagerTest, NotEnoughSpace) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  // Mock Drive search to return 3 unpinned files that total just above 512 MB.
  // The available space of 2.5 GB is not enough if you take in account the 2 GB
  // margin.
  const vector<DriveItem> items = {
      {.size = int64_t(300) << 20}, {.size = int64_t(212) << 20}, {.size = 1}};

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items), Return(kFileOk)))
      .WillOnce(DoAll(PopulateNoSearchItems(), Return(kFileOk)));
  EXPECT_CALL(completion_callback, Run(Stage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(int64_t(2560) << 20));  // 2.5 GB.

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  manager.Start();
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, int64_t(2560) << 20);
  EXPECT_EQ(progress.required_space, (int64_t(512) << 20) + (int64_t(4) << 10));
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests what happens when PinningManager cannot get enough free space during
// the periodic check.
TEST_F(DriveFsPinningManagerTest, NotEnoughSpace2) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(completion_callback, Run(Stage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(200 << 20));  // 200 MB

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
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

// Tests what happens when PinningManager cannot get enough free space that has
// been emitted by the `LowDiskSpace` message sent via cryptohome UserDataAuth
// service.
TEST_F(DriveFsPinningManagerTest, NotEnoughSpace3) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  EXPECT_CALL(completion_callback, Run(Stage::kNotEnoughSpace))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.SetCompletionCallback(completion_callback.Get());
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  FakeUserDataAuthClient::Get()->NotifyLowDiskSpace(200 << 20);
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(progress.free_space, 200 << 20);
  EXPECT_EQ(progress.required_space, 0);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

TEST_F(DriveFsPinningManagerTest, OnSpaceUpdate) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;

  SpacedClient::Observer::SpaceEvent event;
  const SpacedClient::Observer::SpaceEvent& cevent = event;

  event.set_free_space_bytes(int64_t(2) << 30);
  manager.OnSpaceUpdate(cevent);
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  EXPECT_EQ(manager.progress_.free_space, int64_t(2) << 30);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);

  EXPECT_FALSE(manager.spaced_client_.IsObserving());
  FakeSpacedClient::Get()->set_connected(true);

  // Transition to kNotEnoughSpace.
  event.set_free_space_bytes(int64_t(1) << 30);
  manager.OnSpaceUpdate(cevent);
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(manager.progress_.free_space, int64_t(1) << 30);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_TRUE(manager.spaced_client_.IsObserving());

  // Still in kNotEnoughSpace.
  event.clear_free_space_bytes();
  manager.OnSpaceUpdate(cevent);
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(manager.progress_.free_space, 0);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_TRUE(manager.spaced_client_.IsObserving());

  // Go back to enough space.
  event.set_free_space_bytes(int64_t(2) << 30);
  manager.OnSpaceUpdate(cevent);
  EXPECT_EQ(manager.progress_.stage, Stage::kSuccess);
  EXPECT_EQ(manager.progress_.free_space, int64_t(2) << 30);
  EXPECT_EQ(manager.progress_.required_space, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_EQ(manager.progress_.pinned_files, 0);
  EXPECT_FALSE(manager.spaced_client_.IsObserving());

  manager.progress_.stage = Stage::kStopped;
}

TEST_F(DriveFsPinningManagerTest, StartMonitoringSpace) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kSyncing;
  EXPECT_FALSE(manager.spaced_client_.IsObserving());

  // If SpacedClient is not connected, then StartMonitoringSpace should fail.
  FakeSpacedClient::Get()->set_connected(false);
  EXPECT_FALSE(manager.StartMonitoringSpace());
  EXPECT_FALSE(manager.spaced_client_.IsObserving());

  // If SpacedClient is connected, then StartMonitoringSpace should succeed.
  FakeSpacedClient::Get()->set_connected(true);
  EXPECT_TRUE(manager.StartMonitoringSpace());
  EXPECT_TRUE(manager.spaced_client_.IsObserving());

  // StartMonitoringSpace called when it is already monitoring.
  EXPECT_TRUE(manager.StartMonitoringSpace());
  EXPECT_TRUE(manager.spaced_client_.IsObserving());

  // Stop monitoring.
  manager.StopMonitoringSpace();
  EXPECT_FALSE(manager.spaced_client_.IsObserving());

  // Stop monitoring when it is already stopped.
  manager.StopMonitoringSpace();
  EXPECT_FALSE(manager.spaced_client_.IsObserving());

  manager.progress_.stage = Stage::kStopped;
}

TEST_F(DriveFsPinningManagerTest, CalculateRequiredSpace) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.SetSpaceGetter(GetSpaceGetter());

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_TRUE(manager.progress_.should_pin);

  // Pin manager not in the right stage to start calculating required space.
  manager.progress_.stage = Stage::kGettingFreeSpace;
  EXPECT_FALSE(manager.CalculateRequiredSpace());
  EXPECT_TRUE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.stage = Stage::kListingFiles;
  EXPECT_FALSE(manager.CalculateRequiredSpace());
  EXPECT_TRUE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);

  manager.progress_.stage = Stage::kSyncing;
  EXPECT_FALSE(manager.CalculateRequiredSpace());
  EXPECT_TRUE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);

  manager.progress_.stage = Stage::kPausedOffline;
  EXPECT_FALSE(manager.CalculateRequiredSpace());
  EXPECT_TRUE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);

  manager.progress_.stage = Stage::kPausedBatterySaver;
  EXPECT_FALSE(manager.CalculateRequiredSpace());
  EXPECT_TRUE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);

  // Pin manager already calculating required space.
  manager.progress_.should_pin = false;
  manager.progress_.stage = Stage::kGettingFreeSpace;
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.stage = Stage::kListingFiles;
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);

  manager.progress_.stage = Stage::kSyncing;
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);

  // Pin manager is stopped. Start calculating required space.
  manager.progress_.should_pin = true;
  manager.progress_.stage = Stage::kStopped;
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.should_pin = true;
  manager.progress_.stage = Stage::kSuccess;
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.should_pin = true;
  manager.progress_.stage = Stage::kCannotListFiles;
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.should_pin = true;
  manager.progress_.stage = Stage::kCannotGetFreeSpace;
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.should_pin = true;
  manager.progress_.stage = Stage::kCannotEnableDocsOffline;
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  EXPECT_FALSE(manager.progress_.should_pin);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.progress_.stage = Stage::kStopped;
}

TEST_F(DriveFsPinningManagerTest, JustCheckRequiredSpace) {
  base::HistogramTester histogram_tester;
  CompletionCallback completion_callback;
  RunLoop run_loop;

  // Mock Drive search to return 2 unpinned files that total to 512 MB. The
  // available space of 2.5GB is just enough if you take in account the 2 GB
  // margin.
  const vector<DriveItem> items = {{.size = 300 << 20}, {.size = 212 << 20}};

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items), Return(kFileOk)))
      .WillOnce(DoAll(PopulateNoSearchItems(), Return(kFileOk)));
  EXPECT_CALL(completion_callback, Run(Stage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(int64_t(2560) << 20));  // 2.5 GB.

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kSuccess);
  EXPECT_EQ(progress.free_space, int64_t(2560) << 20);
  EXPECT_EQ(progress.required_space, 512 << 20);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
  histogram_tester.ExpectUniqueTimeSample(
      "FileBrowser.GoogleDrive.BulkPinning.TimeSpentListing",
      progress.time_spent_listing_items, 1);
}

TEST_F(DriveFsPinningManagerTest, WhenMoreResultsReturnedNextPageIsAttempted) {
  CompletionCallback completion_callback;
  RunLoop run_loop;

  const vector<DriveItem> items_1 = {{.size = 100 << 20}, {.size = 100 << 20}};
  const vector<DriveItem> items_2 = {{.size = 100 << 20}, {.size = 100 << 20}};

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(DoAll(PopulateSearchItems(items_1), Return(kFileOk)))
      .WillOnce(DoAll(PopulateNoSearchItems(),
                      Return(FileError::FILE_ERROR_OK_WITH_MORE_RESULTS)))
      .WillOnce(DoAll(PopulateSearchItems(items_2), Return(kFileOk)))
      .WillOnce(DoAll(PopulateNoSearchItems(), Return(kFileOk)));
  EXPECT_CALL(completion_callback, Run(Stage::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _))
      .WillOnce(RunOnceCallback<1>(int64_t(2560) << 20));  // 2.5 GB.

  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.SetSpaceGetter(GetSpaceGetter());
  manager.SetCompletionCallback(completion_callback.Get());
  EXPECT_TRUE(manager.CalculateRequiredSpace());
  run_loop.Run();

  const Progress progress = manager.GetProgress();
  EXPECT_EQ(progress.stage, Stage::kSuccess);
  EXPECT_EQ(progress.free_space, int64_t(2560) << 20);
  EXPECT_EQ(progress.required_space, 400 << 20);
  EXPECT_EQ(progress.pinned_bytes, 0);
  EXPECT_EQ(progress.pinned_files, 0);
}

// Tests PinningManager::SetOnline() and BatterySaverModeStateChanged().
TEST_F(DriveFsPinningManagerTest, SetOnlineAndBatteryOk) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.SetSpaceGetter(GetSpaceGetter());

  auto set_online = [&](bool online) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
    manager.SetOnline(online);
    EXPECT_EQ(manager.is_online_, online);
  };

  auto set_battery_ok = [&](bool ok) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
    power_manager::BatterySaverModeState state;
    state.set_enabled(!ok);
    manager.BatterySaverModeStateChanged(state);
    EXPECT_EQ(manager.is_battery_ok_, ok);
  };

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  EXPECT_TRUE(manager.is_online_);
  EXPECT_TRUE(manager.is_battery_ok_);

  // Online or battery change before starting should remain in Stopped state.
  set_online(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  set_online(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  set_battery_ok(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
  set_battery_ok(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  // Start when not online should go to PausedOffline.
  set_online(false);
  set_battery_ok(true);
  manager.Start();
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  manager.Stop();
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  // Start when battery saver should go to PausedBatterySaver.
  set_online(true);
  set_battery_ok(false);
  manager.Start();
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  manager.Stop();
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);

  // Start should go to state GettingFreeSpace.
  set_online(true);
  set_battery_ok(true);
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  manager.Start();
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  // Once started, going offline should toggle pause.
  set_online(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);
  set_online(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  set_online(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  // Once started, battery saver should toggle pause.
  set_battery_ok(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);
  set_battery_ok(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  set_battery_ok(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  // When both offline and battery saver are set, we should remain in the
  // current Paused state. Resolving one of them should leave us in correct
  // Paused state.
  set_online(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  set_battery_ok(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  set_battery_ok(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  set_battery_ok(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  set_online(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  set_battery_ok(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);
  set_battery_ok(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  set_online(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  set_online(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  set_online(false);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedBatterySaver);
  set_battery_ok(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kPausedOffline);
  EXPECT_CALL(space_getter_, GetFreeSpace(gcache_dir_, _)).Times(1);
  set_online(true);
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  // Stop should go to state Stopped.
  manager.Stop();
  EXPECT_EQ(manager.progress_.stage, Stage::kStopped);
}

// Tests PinningManager::HandleQueryItem().
TEST_F(DriveFsPinningManagerTest, HandleQueryItem) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;
  manager.progress_.max_active_queries = 2;
  manager.progress_.active_queries = 2;
  manager.progress_.total_queries = 2;

  const auto reset = [&manager, saved_progress = manager.progress_]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
    manager.progress_ = saved_progress;
    manager.listed_items_.clear();
    manager.files_to_pin_.clear();
    manager.files_to_track_.clear();
  };

  const Id dir_id = Id(101);
  const Path dir_path("/root/Folder");
  const Path absolute_dir_path(mount_path_.Append("root/Folder"));
  QueryItem item;
  item.path = Path("/root/Folder/Item");
  item.metadata = FileMetadata::New();
  FileMetadata& md = *item.metadata;
  md.stable_id = 102;
  md.size = 0;
  md.pinned = false;
  md.available_offline = false;
  md.capabilities = mojom::Capabilities::New();

  // Unexpected item type.
  md.type = FileMetadata::Type(-1);

  EXPECT_EQ(manager.progress_.skipped_items, 0);
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));
  reset();

  // Broken shortcuts.
  using LookupStatus = mojom::ShortcutDetails::LookupStatus;
  md.shortcut_details = mojom::ShortcutDetails::New();
  md.type = FileMetadata::Type::kFile;
  md.shortcut_details->target_stable_id = 201;
  md.shortcut_details->target_lookup_status = LookupStatus::kUnknown;
  md.stable_id++;

  EXPECT_EQ(manager.progress_.listed_shortcuts, 0);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);

  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 1);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(0));
  reset();

  md.shortcut_details->target_lookup_status = LookupStatus::kPermissionDenied;
  md.shortcut_details->target_stable_id++;
  md.stable_id++;
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 1);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(0));
  reset();

  md.shortcut_details->target_lookup_status = LookupStatus::kNotFound;
  md.shortcut_details->target_stable_id++;
  md.stable_id++;
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 1);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(0));
  reset();

  // Shortcut to trashed item.
  md.shortcut_details->target_lookup_status = LookupStatus::kOk;
  md.shortcut_details->target_stable_id++;
  md.trashed = true;
  md.stable_id++;
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 1);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(0));
  reset();
  md.trashed = false;

  // Valid shortcut to file.
  const Id target_id = Id(++md.shortcut_details->target_stable_id);
  const Id stable_id = Id(++md.stable_id);
  ASSERT_NE(target_id, stable_id);
  md.shortcut_details->target_lookup_status = LookupStatus::kOk;
  md.type = FileMetadata::Type::kFile;
  md.size = 10000;
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_FALSE(md.shortcut_details);
  EXPECT_EQ(Id(md.stable_id), target_id);
  EXPECT_EQ(manager.progress_.skipped_items, 0);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 1);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));
  EXPECT_THAT(manager.listed_items_,
              UnorderedElementsAre<PinningManager::ListedItems::value_type>(
                  {target_id, dir_id}));
  EXPECT_THAT(manager.files_to_pin_, UnorderedElementsAre(target_id));
  reset();

  // Shortcut to hosted doc, that gets skipped.
  md.shortcut_details = mojom::ShortcutDetails::New();
  md.shortcut_details->target_lookup_status = LookupStatus::kOk;
  md.shortcut_details->target_stable_id = static_cast<int64_t>(target_id);
  md.stable_id = static_cast<int64_t>(stable_id);
  md.type = FileMetadata::Type::kHosted;
  md.size = 0;
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_FALSE(md.shortcut_details);
  EXPECT_EQ(Id(md.stable_id), target_id);
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 1);
  EXPECT_EQ(manager.progress_.pinned_bytes, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));
  EXPECT_THAT(manager.listed_items_,
              UnorderedElementsAre<PinningManager::ListedItems::value_type>(
                  {target_id, dir_id}));
  EXPECT_THAT(manager.files_to_pin_, SizeIs(0));
  reset();

  // Valid shortcut to directory to directory outside My drive.
  md.shortcut_details = mojom::ShortcutDetails::New();
  md.shortcut_details->target_lookup_status = LookupStatus::kOk;
  md.shortcut_details->target_stable_id = static_cast<int64_t>(target_id);
  md.shortcut_details->target_path =
      temp_dir_.GetPath().Append(".shortcuts-by-id/target_dir");
  md.stable_id = static_cast<int64_t>(stable_id);
  md.type = FileMetadata::Type::kDirectory;
  item.path = Path("/root/Folder");
  md.size = 0;
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 1);
  EXPECT_EQ(manager.progress_.broken_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(0));
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_TRUE(md.shortcut_details);
  EXPECT_NE(Id(md.stable_id), target_id);
  reset();

  // Unexpected path
  md.shortcut_details.reset();
  md.type = FileMetadata::Type::kFile;
  md.stable_id = static_cast<int64_t>(stable_id);
  item.path = Path("/root/Unexpected");

  EXPECT_EQ(manager.progress_.files_to_pin, 0);
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 0);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 1);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_EQ(manager.progress_.files_to_pin, 1);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));
  reset();

  // File
  md.stable_id++;
  md.type = FileMetadata::Type::kFile;
  item.path = Path("/root/Folder/File");

  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 0);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 1);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_EQ(manager.progress_.files_to_pin, 1);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));
  reset();

  // Hosted doc
  md.stable_id++;
  md.type = FileMetadata::Type::kHosted;
  item.path = Path("/root/Folder/Doc");

  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 0);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 1);
  EXPECT_EQ(manager.progress_.files_to_pin, 0);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));
  reset();

  // Directory
  md.stable_id++;
  md.type = FileMetadata::Type::kDirectory;
  item.path = Path("/root/Folder/Dir");

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 0);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 1);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_EQ(manager.progress_.files_to_pin, 0);
  EXPECT_EQ(manager.progress_.max_active_queries, 3);
  EXPECT_EQ(manager.progress_.active_queries, 3);
  EXPECT_EQ(manager.progress_.total_queries, 3);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));

  // Already visited directory
  manager.HandleQueryItem(dir_id, dir_path, std::as_const(item));
  EXPECT_EQ(manager.progress_.skipped_items, 1);
  EXPECT_EQ(manager.progress_.listed_shortcuts, 0);
  EXPECT_EQ(manager.progress_.listed_dirs, 1);
  EXPECT_EQ(manager.progress_.listed_files, 0);
  EXPECT_EQ(manager.progress_.listed_docs, 0);
  EXPECT_EQ(manager.progress_.files_to_pin, 0);
  EXPECT_EQ(manager.progress_.max_active_queries, 3);
  EXPECT_EQ(manager.progress_.active_queries, 3);
  EXPECT_EQ(manager.progress_.total_queries, 3);
  EXPECT_THAT(manager.listed_items_, SizeIs(1));

  manager.Stop();
}

// Tests PinningManager::OnNextPage() when a query is dropped before the
// response is received.
TEST_F(DriveFsPinningManagerTest, DropQuery) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;
  manager.progress_.max_active_queries = 2;
  manager.progress_.active_queries = 2;

  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(
          Invoke([&manager](std::optional<vector<QueryItemPtr>>* const items) {
            manager.Stop();
            *items = {};
            return FileError::FILE_ERROR_OK;
          }));

  PinningManager::Query query;
  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  drivefs_.StartSearchQuery(query.BindNewPipeAndPassReceiver(),
                            QueryParameters::New());

  manager.GetNextPage(Id(101), Path("/root/My Folder"), std::move(query));
  task_environment_.RunUntilIdle();
}

// Tests PinningManager::OnSearchResult() when a query finishes and there are
// still other active queries.
TEST_F(DriveFsPinningManagerTest, OnSearchResult) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;
  manager.progress_.max_active_queries = 2;
  manager.progress_.active_queries = 2;

  manager.OnSearchResult(Id(101), Path("/root/My Folder"),
                         PinningManager::Query(), FileError::FILE_ERROR_OK, {});

  EXPECT_EQ(manager.progress_.stage, Stage::kListingFiles);
  EXPECT_EQ(manager.progress_.max_active_queries, 2);
  EXPECT_EQ(manager.progress_.active_queries, 1);

  manager.OnSearchResult(Id(100), Path("/root"), PinningManager::Query(),
                         FileError::FILE_ERROR_OK, {});

  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);
  EXPECT_EQ(manager.progress_.max_active_queries, 2);
  EXPECT_EQ(manager.progress_.active_queries, 0);

  manager.Stop();
}

// Tests PinningManager::OnSearchResult() with transient errors.
TEST_F(DriveFsPinningManagerTest, OnTransientError) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;

  EXPECT_CALL(drivefs_, OnStartSearchQuery(_)).Times(1);
  EXPECT_CALL(drivefs_, OnGetNextPage(_))
      .WillOnce(Return(FileError::FILE_ERROR_NO_CONNECTION));
  manager.ListItems(Id::kNone, Path("/root"));
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

// Tests PinningManager::OnError().
TEST_F(DriveFsPinningManagerTest, OnError) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

  // Stopping the pin manager should retain the existing error stage.
  manager.Stop();
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);

  // Error of type kPinningFailedDiskFull should not have any effect if the pin
  // manager is already stopped.
  manager.OnError(std::as_const(error));
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);
}

// Tests that calling PinningManager::Start() when the PinningManager is already
// in progress does not have any effect.
TEST_F(DriveFsPinningManagerTest, StartWhenInProgress) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kGettingFreeSpace;
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.Start();
  EXPECT_EQ(manager.progress_.stage, Stage::kGettingFreeSpace);

  manager.Stop();
}

// Tests PinningManager::StartPinning().
TEST_F(DriveFsPinningManagerTest, StartPinning) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  manager.progress_.stage = Stage::kListingFiles;
  DCHECK_EQ(manager.progress_.free_space, 0);

  manager.StartPinning();
  EXPECT_EQ(manager.progress_.stage, Stage::kNotEnoughSpace);

  manager.progress_.stage = Stage::kListingFiles;
  manager.progress_.free_space = int64_t(4) << 30;  // 4 GB
  manager.progress_.should_pin = false;

  EXPECT_CALL(drivefs_, SetDocsOfflineEnabled(true, _))
      .Times(1)
      .WillOnce(RunOnceCallback<1>(drive::FILE_ERROR_OK,
                                   DocsOfflineEnableStatus::kSuccess));
  manager.StartPinning();
  EXPECT_EQ(manager.progress_.stage, Stage::kSuccess);

  manager.progress_.stage = Stage::kListingFiles;
  manager.progress_.should_pin = true;

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

// Tests PinningManager::PinSomeFiles().
TEST_F(DriveFsPinningManagerTest, PinSomeFiles) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

  // Add manager.queue_size_ + 20 items to pin.
  for (int i = 0; i < manager.queue_size_ + 20; ++i) {
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

  EXPECT_THAT(manager.files_to_pin_, SizeIs(manager.queue_size_ + 20));
  EXPECT_THAT(manager.files_to_track_, SizeIs(manager.queue_size_ + 21));
  EXPECT_EQ(manager.progress_.syncing_files, 1);

  EXPECT_CALL(drivefs_, SetPinnedByStableId(_, true, _))
      .Times(manager.queue_size_ - 1);
  manager.PinSomeFiles();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  EXPECT_THAT(manager.files_to_pin_, SizeIs(21));
  EXPECT_THAT(manager.files_to_track_, SizeIs(manager.queue_size_ + 21));
  EXPECT_EQ(manager.progress_.syncing_files, manager.queue_size_);

  // Remove 30 files from the set of files to track.
  {
    std::vector<Id> pinned_ids;
    pinned_ids.reserve(manager.files_to_track_.size());
    for (const auto& [id, file] : manager.files_to_track_) {
      if (file.pinned) {
        pinned_ids.push_back(id);
      }
    }

    EXPECT_THAT(pinned_ids, SizeIs(manager.queue_size_));
    pinned_ids.resize(30);
    for (const Id id : pinned_ids) {
      manager.Remove(id, Path(), 0);
    }
  }

  EXPECT_THAT(manager.files_to_pin_, SizeIs(21));
  EXPECT_THAT(manager.files_to_track_, SizeIs(manager.queue_size_ - 9));
  EXPECT_EQ(manager.progress_.syncing_files, manager.queue_size_ - 30);

  EXPECT_CALL(drivefs_, SetPinnedByStableId(_, true, _)).Times(21);
  manager.PinSomeFiles();
  EXPECT_EQ(manager.progress_.stage, Stage::kSyncing);
  EXPECT_THAT(manager.files_to_pin_, IsEmpty());
  EXPECT_THAT(manager.files_to_track_, SizeIs(manager.queue_size_ - 9));
  EXPECT_EQ(manager.progress_.syncing_files, manager.queue_size_ - 9);

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

// Tests PinningManager::CheckStalledFiles().
TEST_F(DriveFsPinningManagerTest, CheckStalledFiles) {
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);

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

// Tests PinningManager::NotifyProgress.
TEST_F(DriveFsPinningManagerTest, NotifyProgress) {
  MockObserver observer;
  PinningManager::Observer observer2;
  PinningManager manager(profile_path_, mount_path_, &drivefs_, kMaxQueueSize);
  manager.AddObserver(&observer);
  manager.AddObserver(&observer2);

  DCHECK_CALLED_ON_VALID_SEQUENCE(manager.sequence_checker_);
  EXPECT_CALL(observer, OnProgress(testing::Ref(manager.progress_))).Times(1);
  manager.NotifyProgress();
  manager.RemoveObserver(&observer);
}

TEST_F(DriveFsPinningManagerTest, IsUntrackedPath) {}

}  // namespace drivefs::pinning
