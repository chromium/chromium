// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <map>
#include <string>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/image_util.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user_names.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace {

// Aliases ---------------------------------------------------------------------

using ::ash::holding_space::ScopedTestMountPoint;
using ::ash::holding_space_metrics::FilePickerBindingContext;
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::BucketsAreArray;
using ::testing::AllOf;
using ::testing::Conditional;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Value;

// Constants -------------------------------------------------------------------

constexpr char kTotalCountV2HistogramPrefix[] =
    "HoldingSpace.Item.TotalCountV2";

// Helpers ---------------------------------------------------------------------

// Returns whether the bitmaps backing the specified `gfx::ImageSkia` are equal.
bool BitmapsAreEqual(const gfx::ImageSkia& a, const gfx::ImageSkia& b) {
  return gfx::BitmapsAreEqual(*a.bitmap(), *b.bitmap());
}

// Creates an empty holding space image.
std::unique_ptr<HoldingSpaceImage> CreateTestHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      holding_space_util::GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

std::unique_ptr<KeyedService> BuildArcFileSystemBridge(
    content::BrowserContext* context) {
  EXPECT_TRUE(arc::ArcServiceManager::Get());
  EXPECT_TRUE(arc::ArcServiceManager::Get()->arc_bridge_service());
  return std::make_unique<arc::ArcFileSystemBridge>(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */,
      disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

HoldingSpaceItem* AddUninitializedItem(HoldingSpaceModel* model,
                                       HoldingSpaceItem::Type type,
                                       const base::FilePath& path) {
  // Create a holding space item and use it to create a serialized item
  // dictionary.
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      type,
      HoldingSpaceFile(path, HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem:ignored")),
      base::BindOnce(&CreateTestHoldingSpaceImage));
  const auto serialized_holding_space_item = item->Serialize();
  auto deserialized_item = HoldingSpaceItem::Deserialize(
      serialized_holding_space_item,
      /*image_resolver=*/base::BindOnce(&CreateTestHoldingSpaceImage));

  auto* deserialized_item_ptr = deserialized_item.get();
  model->AddItem(std::move(deserialized_item));
  return deserialized_item_ptr;
}

// Returns the expected TotalCountV2 histogram samples for the specified
// `model`. The names of histograms returned are:
// * "HoldingSpace.Item.TotalCountV2.All"
// * "HoldingSpace.Item.TotalCountV2.All.FileSystemType.{fs_type}"
// * "HoldingSpace.Item.TotalCountV2.{type}"
// * "HoldingSpace.Item.TotalCountV2.{type}.FileSystemType.{fs_type}"
std::map<std::string, std::vector<Bucket>>
GetExpectedTotalCountV2HistogramSamples(const HoldingSpaceModel* model) {
  // Aliases.
  using FileSystemType = HoldingSpaceFile::FileSystemType;
  using Type = HoldingSpaceItem::Type;

  std::map<std::string, std::vector<Bucket>> result;

  // Fill "HoldingSpace.Item.TotalCountV2.All".
  result.emplace(base::StrCat({kTotalCountV2HistogramPrefix, ".All"}),
                 std::vector<Bucket>(
                     {Bucket(/*sample=*/model->items().size(), /*count=*/1u)}));

  // File system types are allowlisted based on need to limit the number of
  // recorded histograms arising from combinations with holding space item type.
  constexpr auto kAllowlistedFsTypes = base::MakeFixedFlatSet<FileSystemType>(
      {FileSystemType::kDriveFs, FileSystemType::kLocal});

  // Fill "HoldingSpace.Item.TotalCountV2.All.FileSystemType.{fs_type}".
  for (const FileSystemType fs_type : kAllowlistedFsTypes) {
    result.emplace(
        base::StrCat({kTotalCountV2HistogramPrefix, ".All.FileSystemType.",
                      holding_space_util::ToString(fs_type)}),
        std::vector<Bucket>({Bucket(/*sample=*/base::ranges::count(
                                        model->items(), fs_type,
                                        [&](const auto& item) {
                                          return item->file().file_system_type;
                                        }),
                                    /*count=*/1u)}));
  }

  // Fill "HoldingSpace.Item.TotalCountV2.{type}".
  for (const Type type : holding_space_util::GetAllItemTypes()) {
    result.emplace(base::StrCat({kTotalCountV2HistogramPrefix, ".",
                                 holding_space_util::ToString(type)}),
                   std::vector<Bucket>({Bucket(
                       /*sample=*/base::ranges::count(model->items(), type,
                                                      &HoldingSpaceItem::type),
                       /*count=*/1u)}));

    // Fill "HoldingSpace.Item.TotalCountV2.{type}.FileSystemType.{fs_type}".
    for (const FileSystemType fs_type : kAllowlistedFsTypes) {
      result.emplace(
          base::StrCat({kTotalCountV2HistogramPrefix, ".",
                        holding_space_util::ToString(type), ".FileSystemType.",
                        holding_space_util::ToString(fs_type)}),
          std::vector<Bucket>({Bucket(
              /*sample=*/base::ranges::count_if(
                  model->items(),
                  [&](const auto& item) {
                    return item->type() == type &&
                           item->file().file_system_type == fs_type;
                  }),
              /*count=*/1u)}));
    }
  }

  return result;
}

// Returns a new map of histogram samples having merged `a` and `b`.
std::map<std::string, std::vector<Bucket>> MergeHistogramSamples(
    const std::map<std::string, std::vector<Bucket>>& a,
    const std::map<std::string, std::vector<Bucket>>& b) {
  std::map<std::string, std::vector<Bucket>> result = a;
  for (const auto& [name, buckets] : b) {
    auto name_it = result.find(name);

    // Case: Name did *not* exist in other map. Add all buckets.
    if (name_it == result.end()) {
      result.emplace(name, buckets);
      continue;
    }

    std::vector<Bucket>& result_buckets = name_it->second;

    // Case: Name *did* exist in other map.
    for (const auto& bucket : buckets) {
      auto bucket_it =
          base::ranges::find(result_buckets, bucket.min, &Bucket::min);

      // Case: Bucket did *not* exist in other map. Add bucket.
      if (bucket_it == result_buckets.end()) {
        result_buckets.emplace_back(bucket);
        continue;
      }

      // Case: Bucket *did* exist in other map. Update bucket.
      bucket_it->count += bucket.count;
    }
  }
  return result;
}

bool ShouldRestoreFromPersistence(HoldingSpaceItem::Type type) {
  if (HoldingSpaceItem::IsSuggestionType(type) &&
      !features::IsHoldingSpaceSuggestionsEnabled()) {
    return false;
  }
  return true;
}

// Waiters ---------------------------------------------------------------------

// Utility class which can wait until a `HoldingSpaceModel` for a given profile
// is attached to the `HoldingSpaceController`.
class HoldingSpaceModelAttachedWaiter : public HoldingSpaceControllerObserver {
 public:
  explicit HoldingSpaceModelAttachedWaiter(Profile* profile)
      : profile_(profile) {
    holding_space_controller_observation_.Observe(
        HoldingSpaceController::Get());
  }

  void Wait() {
    if (IsModelAttached()) {
      return;
    }

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
  }

 private:
  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override {
    if (wait_loop_ && IsModelAttached()) {
      wait_loop_->Quit();
    }
  }

  bool IsModelAttached() const {
    HoldingSpaceKeyedService* const holding_space_service =
        HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile_);
    return HoldingSpaceController::Get()->model() ==
           holding_space_service->model_for_testing();
  }

  const raw_ptr<Profile> profile_;
  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      holding_space_controller_observation_{this};
  std::unique_ptr<base::RunLoop> wait_loop_;
};

class ItemUpdatedWaiter : public HoldingSpaceModelObserver {
 public:
  ItemUpdatedWaiter(HoldingSpaceModel* model, const HoldingSpaceItem* item)
      : wait_item_(item) {
    model_observer_.Observe(model);
  }

  ItemUpdatedWaiter(const ItemUpdatedWaiter&) = delete;
  ItemUpdatedWaiter& operator=(const ItemUpdatedWaiter&) = delete;
  ~ItemUpdatedWaiter() override = default;

  void Wait() {
    ASSERT_TRUE(wait_item_);
    ASSERT_FALSE(wait_loop_);
    if (wait_item_updated_) {
      // The item has already been updated, no waiting necessary.
      wait_item_updated_ = false;
      return;
    }

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
  }

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override {
    if (!wait_loop_) {
      // `wait_loop_` is nullptr, if wait has not yet been called.
      if (item == wait_item_) {
        wait_item_updated_ = true;
      }
      return;
    }
    if (item == wait_item_) {
      wait_loop_->Quit();
    }
  }

  raw_ptr<const HoldingSpaceItem> wait_item_ = nullptr;
  std::unique_ptr<base::RunLoop> wait_loop_;
  bool wait_item_updated_ = false;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};
};

class ItemRemovedWaiter : public HoldingSpaceModelObserver {
 public:
  ItemRemovedWaiter(HoldingSpaceModel* model, const HoldingSpaceItem* item)
      : wait_item_(item) {
    model_observer_.Observe(model);
  }

  ItemRemovedWaiter(const ItemRemovedWaiter&) = delete;
  ItemRemovedWaiter& operator=(const ItemRemovedWaiter&) = delete;
  ~ItemRemovedWaiter() override = default;

  void Wait() {
    ASSERT_TRUE(wait_item_);
    ASSERT_FALSE(wait_loop_);
    if (wait_item_removed_) {
      // The item has already been removed, no waiting necessary.
      wait_item_removed_ = false;
      return;
    }

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
  }

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    if (items.size() != 1 || items[0] != wait_item_) {
      return;
    }

    if (wait_loop_) {
      wait_loop_->Quit();
    } else {
      wait_item_removed_ = true;
    }
  }

  raw_ptr<const HoldingSpaceItem, DanglingUntriaged> wait_item_ = nullptr;
  std::unique_ptr<base::RunLoop> wait_loop_;
  bool wait_item_removed_ = false;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};
};

class ItemsInitializedWaiter : public HoldingSpaceModelObserver {
 public:
  // Predicate that determines whether the waiter should wait for an item to be
  // initialized.
  using ItemFilter =
      base::RepeatingCallback<bool(const HoldingSpaceItem* item)>;

  explicit ItemsInitializedWaiter(HoldingSpaceModel* model) : model_(model) {}
  ItemsInitializedWaiter(const ItemsInitializedWaiter&) = delete;
  ItemsInitializedWaiter& operator=(const ItemsInitializedWaiter&) = delete;
  ~ItemsInitializedWaiter() override = default;

  // NOTE: The filter defaults to all items.
  void Wait(const ItemFilter& filter = ItemFilter()) {
    ASSERT_FALSE(wait_loop_);

    filter_ = filter;
    if (FilteredItemsInitialized()) {
      return;
    }

    base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
        model_observer{this};
    model_observer.Observe(model_.get());

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
    filter_ = ItemFilter();
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    if (FilteredItemsInitialized()) {
      wait_loop_->Quit();
    }
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    if (FilteredItemsInitialized()) {
      wait_loop_->Quit();
    }
  }

 private:
  bool FilteredItemsInitialized() const {
    for (auto& item : model_->items()) {
      if (filter_ && !filter_.Run(item.get())) {
        continue;
      }
      if (!item->IsInitialized()) {
        return false;
      }
    }
    return true;
  }

  const raw_ptr<HoldingSpaceModel> model_;
  ItemFilter filter_;
  std::unique_ptr<base::RunLoop> wait_loop_;
};

class ItemImageUpdateWaiter {
 public:
  explicit ItemImageUpdateWaiter(const HoldingSpaceItem* item) {
    image_subscription_ =
        item->image().AddImageSkiaChangedCallback(base::BindRepeating(
            &ItemImageUpdateWaiter::OnHoldingSpaceItemImageChanged,
            base::Unretained(this)));
  }
  ItemImageUpdateWaiter(const ItemImageUpdateWaiter&) = delete;
  ItemImageUpdateWaiter& operator=(const ItemImageUpdateWaiter&) = delete;
  ~ItemImageUpdateWaiter() = default;

  void Wait() { run_loop_.Run(); }

 private:
  void OnHoldingSpaceItemImageChanged() { run_loop_.Quit(); }

  base::RunLoop run_loop_;
  base::CallbackListSubscription image_subscription_;
};

// Mocks -----------------------------------------------------------------------

// A mock `content::DownloadManager` which can notify observers of events.
class MockDownloadManager : public content::MockDownloadManager {
 public:
  // content::MockDownloadManager:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.ManagerGoingDown(this);
    }
  }

  void NotifyDownloadCreated(download::DownloadItem* item) {
    for (auto& observer : observers_) {
      observer.OnDownloadCreated(this, item);
    }
  }

 private:
  base::ObserverList<content::DownloadManager::Observer>::Unchecked observers_;
};

}  // namespace

// HoldingSpaceKeyedServiceTest ------------------------------------------------

class HoldingSpaceKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  HoldingSpaceKeyedServiceTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(true);
  }

  HoldingSpaceKeyedServiceTest(const HoldingSpaceKeyedServiceTest& other) =
      delete;
  HoldingSpaceKeyedServiceTest& operator=(
      const HoldingSpaceKeyedServiceTest& other) = delete;
  ~HoldingSpaceKeyedServiceTest() override {
    HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(false);
  }

  // BrowserWithTestWindowTest:
  void SetUp() override {
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(true);

    // The test's task environment starts with a mock time close to the Unix
    // epoch, but the files that back holding space items are created with
    // accurate timestamps. Advance the clock so that the test's mock time and
    // the time used for file operations are in sync for file age calculations.
    task_environment()->AdvanceClock(base::subtle::TimeNowIgnoringOverride() -
                                     base::Time::Now());
    // Needed by `file_manager::VolumeManager`.
    disks::DiskMountManager::InitializeForTesting(
        new disks::FakeDiskMountManager);

    // Needed for `app_list::MockFileSuggestKeyedService`.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    BrowserWithTestWindowTest::SetUp();

    WaitUntilFileSuggestServiceReady(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            GetProfile()));
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    disks::DiskMountManager::Shutdown();

    ash::ProfileHelper::SetProfileToUserForTestingEnabled(false);
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        TestingProfile::TestingFactory{
            arc::ArcFileSystemBridge::GetFactory(),
            base::BindRepeating(&BuildArcFileSystemBridge)},
        TestingProfile::TestingFactory{
            file_manager::VolumeManagerFactory::GetInstance(),
            base::BindRepeating(&BuildVolumeManager)},
        TestingProfile::TestingFactory{
            FileSuggestKeyedServiceFactory::GetInstance(),
            base::BindRepeating(
                &MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService,
                temp_dir_.GetPath())}};
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile = BrowserWithTestWindowTest::CreateProfile(profile_name);
    SetUpDownloadManager(profile);
    return profile;
  }

  TestingProfile* CreateSecondaryProfile(
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs = nullptr) {
    constexpr char kSecondaryProfileName[] = "secondary_profile";
    LogIn(kSecondaryProfileName);
    auto* profile = profile_manager()->CreateTestingProfile(
        kSecondaryProfileName, std::move(prefs), /*user_name=*/std::u16string(),
        /*avatar_id=*/0, GetTestingFactories());
    OnUserProfileCreated(kSecondaryProfileName, profile);
    return profile;
  }

  using PopulatePrefStoreCallback = base::OnceCallback<void(TestingPrefStore*)>;
  TestingProfile* CreateSecondaryProfile(PopulatePrefStoreCallback callback) {
    // Create and initialize pref registry.
    auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    RegisterUserProfilePrefs(registry.get());

    // Create and initialize pref store.
    auto pref_store = base::MakeRefCounted<TestingPrefStore>();
    std::move(callback).Run(pref_store.get());

    // Create and initialize pref factory.
    sync_preferences::PrefServiceMockFactory prefs_factory;
    prefs_factory.set_user_prefs(pref_store);

    // Create and return profile.
    return CreateSecondaryProfile(prefs_factory.CreateSyncable(registry));
  }

  void ActivateSecondaryProfile() {
    const std::string kSecondaryProfileName = "secondary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    GetSessionControllerClient()->AddUserSession(kSecondaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  TestSessionControllerClient* GetSessionControllerClient() {
    return ash_test_helper()->test_session_controller_client();
  }

  // Resolves an absolute file path in the file manager's file system context,
  // and returns the file's file system URL.
  GURL GetFileSystemUrl(Profile* profile,
                        const base::FilePath& absolute_file_path) {
    GURL file_system_url;
    EXPECT_TRUE(file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile, absolute_file_path, file_manager::util::GetFileManagerURL(),
        &file_system_url));
    return file_system_url;
  }

  // Resolves a file system URL in the file manager's file system context, and
  // returns the file's virtual path relative to the mount point root.
  // Returns an empty file if the URL cannot be resolved to a file. For example,
  // if it's not well formed, or the file manager app cannot access it.
  base::FilePath GetVirtualPathFromUrl(
      const GURL& url,
      const std::string& expected_mount_point) {
    storage::FileSystemContext* fs_context =
        file_manager::util::GetFileManagerFileSystemContext(GetProfile());
    storage::FileSystemURL fs_url =
        fs_context->CrackURLInFirstPartyContext(url);

    base::RunLoop run_loop;
    base::FilePath result;
    base::FilePath* result_ptr = &result;
    fs_context->ResolveURL(
        fs_url,
        base::BindLambdaForTesting(
            [&run_loop, &expected_mount_point, &result_ptr](
                base::File::Error result, const storage::FileSystemInfo& info,
                const base::FilePath& file_path,
                storage::FileSystemContext::ResolvedEntryType type) {
              EXPECT_EQ(base::File::Error::FILE_OK, result);
              EXPECT_EQ(storage::FileSystemContext::RESOLVED_ENTRY_FILE, type);
              if (expected_mount_point == info.name) {
                *result_ptr = file_path;
              } else {
                ADD_FAILURE() << "Mount point name '" << info.name
                              << "' does not match expected '"
                              << expected_mount_point << "'";
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  // Creates and returns a fake download item for `profile` with the specified
  // `state`, `file_path`, `target_file_path`, `received_bytes`, and
  // `total_bytes`.
  std::unique_ptr<content::FakeDownloadItem> CreateFakeDownloadItem(
      Profile* profile,
      download::DownloadItem::DownloadState state,
      const base::FilePath& file_path,
      const base::FilePath& target_file_path,
      int64_t received_bytes,
      int64_t total_bytes) {
    auto fake_download_item = std::make_unique<content::FakeDownloadItem>();
    fake_download_item->SetDummyFilePath(file_path);
    fake_download_item->SetReceivedBytes(received_bytes);
    fake_download_item->SetState(state);
    fake_download_item->SetTargetFilePath(target_file_path);
    fake_download_item->SetTotalBytes(total_bytes);

    // Notify observers of the created download.
    download_managers_[profile]->NotifyDownloadCreated(
        fake_download_item.get());
    return fake_download_item;
  }

 protected:
  // Creates a `MockDownloadManager` for `profile` to use.
  void SetUpDownloadManager(Profile* profile) {
    auto manager = std::make_unique<testing::NiceMock<MockDownloadManager>>();
    ON_CALL(*manager, IsManagerInitialized)
        .WillByDefault(testing::Return(true));
    download_managers_[profile] = manager.get();
    profile->SetDownloadManagerForTesting(std::move(manager));
  }

 private:
  std::map<Profile*,
           raw_ptr<testing::NiceMock<MockDownloadManager>, CtnExperimental>>
      download_managers_;
  arc::ArcServiceManager arc_service_manager_;
  base::ScopedTempDir temp_dir_;
};

class HoldingSpaceKeyedServiceWithExperimentalFeatureTest
    : public HoldingSpaceKeyedServiceTest,
      public testing::WithParamInterface<
          /*enable_suggestions=*/bool> {
 public:
  HoldingSpaceKeyedServiceWithExperimentalFeatureTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (GetParam() ? enabled_features : disabled_features)
        .push_back(features::kHoldingSpaceSuggestions);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
                         /*enabled_suggestions=*/testing::Bool());

class HoldingSpaceKeyedServiceWithExperimentalFeatureForGuestTest
    : public HoldingSpaceKeyedServiceWithExperimentalFeatureTest {
 public:
  HoldingSpaceKeyedServiceWithExperimentalFeatureForGuestTest() {
    // To let ProfileHelper::GetUserByProfile() directly return
    // the created guest user, without faking directory paths.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
  }

  void TearDown() override {
    profile_.reset();
    HoldingSpaceKeyedServiceWithExperimentalFeatureTest::TearDown();
  }

  std::string GetDefaultProfileName() override {
    return user_manager::kGuestUserName;
  }

  void LogIn(const std::string& email) override {
    CHECK_EQ(email, user_manager::kGuestUserName);
    auto account_id = user_manager::GuestAccountId();

    user_manager()->AddGuestUser(account_id);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    CHECK_EQ(profile_name, user_manager::kGuestUserName);
    CHECK(!profile_);

    // Construct a guest session profile.
    // Profile is created outside of TestingProfileManager management
    // to inject more factories.
    TestingProfile::Builder guest_profile_builder;
    guest_profile_builder.SetGuestSession();
    guest_profile_builder.SetProfileName(profile_name);
    guest_profile_builder.AddTestingFactories(
        {TestingProfile::TestingFactory{
             arc::ArcFileSystemBridge::GetFactory(),
             base::BindRepeating(&BuildArcFileSystemBridge)},
         TestingProfile::TestingFactory{
             file_manager::VolumeManagerFactory::GetInstance(),
             base::BindRepeating(&BuildVolumeManager)}});
    profile_ = guest_profile_builder.Build();
    OnUserProfileCreated(profile_name, profile_.get());
    return profile_.get();
  }

  std::unique_ptr<Browser> CreateBrowser(
      Profile* profile,
      Browser::Type browser_type,
      bool hosted_app,
      BrowserWindow* browser_window) override {
    // Do not create browser.
    return nullptr;
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceKeyedServiceWithExperimentalFeatureForGuestTest,
    /*enabled_suggestions=*/testing::Bool());

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureForGuestTest,
       GuestUserProfile) {
  auto* guest_profile = profile();

  // Service instances should be created for guest sessions but note that the
  // service factory will redirect to use the primary OTR profile.
  ASSERT_TRUE(guest_profile);
  ASSERT_FALSE(guest_profile->IsOffTheRecord());
  HoldingSpaceKeyedService* const guest_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(guest_profile);
  ASSERT_TRUE(guest_profile_service);

  // Since the service factory redirects to use the primary OTR profile in the
  // case of guest sessions, retrieving the service instance for the primary OTR
  // profile should yield the same result as retrieving the service instance for
  // a non-OTR guest session profile.
  ASSERT_TRUE(guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  HoldingSpaceKeyedService* const primary_otr_guest_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  ASSERT_EQ(guest_profile_service, primary_otr_guest_profile_service);

  // Construct a second OTR profile from `guest_profile`.
  TestingProfile::Builder secondary_otr_guest_profile_builder;
  secondary_otr_guest_profile_builder.SetGuestSession();
  secondary_otr_guest_profile_builder.SetProfileName(
      guest_profile->GetProfileUserName());
  TestingProfile* const secondary_otr_guest_profile =
      secondary_otr_guest_profile_builder.BuildOffTheRecord(
          guest_profile, Profile::OTRProfileID::CreateUniqueForTesting());
  ASSERT_TRUE(secondary_otr_guest_profile);
  ASSERT_TRUE(secondary_otr_guest_profile->IsOffTheRecord());

  // Service instances should be created for non-primary OTR guest session
  // profiles but as stated earlier the service factory will redirect to use the
  // primary OTR profile. This means that the secondary OTR profile service
  // instance should be equal to that explicitly created for the primary OTR
  // profile.
  HoldingSpaceKeyedService* const secondary_otr_guest_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_otr_guest_profile);
  ASSERT_TRUE(secondary_otr_guest_profile_service);
  ASSERT_EQ(primary_otr_guest_profile_service,
            secondary_otr_guest_profile_service);
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       OffTheRecordProfile) {
  // Service instances should be created for on the record profiles.
  HoldingSpaceKeyedService* const primary_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  ASSERT_TRUE(primary_profile_service);

  // Construct an incognito profile from the primary profile.
  TestingProfile::Builder incognito_primary_profile_builder;
  incognito_primary_profile_builder.SetProfileName(
      GetProfile()->GetProfileUserName());
  Profile* const incognito_primary_profile =
      incognito_primary_profile_builder.BuildIncognito(GetProfile());
  ASSERT_TRUE(incognito_primary_profile);
  ASSERT_TRUE(incognito_primary_profile->IsOffTheRecord());

  // Service instances should *not* typically be created for OTR profiles. The
  // once exception is for guest users who redirect to use original profile.
  HoldingSpaceKeyedService* const incognito_primary_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          incognito_primary_profile);
  ASSERT_FALSE(incognito_primary_profile_service);
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       SecondaryUserProfile) {
  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  TestingProfile* const second_profile = CreateSecondaryProfile();
  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          second_profile);

  // Just creating a secondary profile shouldn't change the active client/model.
  EXPECT_EQ(HoldingSpaceController::Get()->client(),
            primary_holding_space_service->client());
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            primary_holding_space_service->model_for_testing());

  // Switching the active user should change the active client/model (multi-user
  // support).
  ActivateSecondaryProfile();
  EXPECT_EQ(HoldingSpaceController::Get()->client(),
            secondary_holding_space_service->client());
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            secondary_holding_space_service->model_for_testing());
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       RecordsUserPreferencesAtStartUp) {
  // Initially expect no user preferences recorded.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "HoldingSpace.UserPreferences.PreviewsEnabled", /*count=*/0u);
  histogram_tester.ExpectTotalCount(
      "HoldingSpace.UserPreferences.SuggestionsExpanded", /*count=*/0u);

  constexpr bool kPreviewsEnabled = false;
  constexpr bool kSuggestionsExpanded = false;

  // Create a profile with explicitly set user preferences.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        pref_store->SetValueSilently(
            "ash.holding_space.previews_enabled", base::Value(kPreviewsEnabled),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
        pref_store->SetValueSilently(
            "ash.holding_space.suggestions_expanded",
            base::Value(kSuggestionsExpanded),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  // Ensure service creation for the created profile.
  HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(secondary_profile);

  // Expect user preferences recorded.
  histogram_tester.ExpectTotalCount(
      "HoldingSpace.UserPreferences.PreviewsEnabled", /*count=*/1u);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.UserPreferences.PreviewsEnabled",
      /*sample=*/kPreviewsEnabled, /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "HoldingSpace.UserPreferences.SuggestionsExpanded", /*count=*/1u);
  histogram_tester.ExpectBucketCount(
      "HoldingSpace.UserPreferences.SuggestionsExpanded",
      /*sample=*/kSuggestionsExpanded,
      /*expected_count=*/1u);
}

// Verifies that updates to the holding space model are persisted.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       UpdatePersistentStorage) {
  // Create a file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  HoldingSpaceModel* const primary_holding_space_model =
      HoldingSpaceController::Get()->model();

  EXPECT_EQ(primary_holding_space_model,
            primary_holding_space_service->model_for_testing());

  base::Value::List persisted_holding_space_items;

  // Verify persistent storage is updated when adding each type of item.
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    const base::FilePath file_path = downloads_mount->CreateArbitraryFile();
    const GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);
    const HoldingSpaceFile::FileSystemType file_system_type =
        holding_space_util::ResolveFileSystemType(GetProfile(),
                                                  file_system_url);

    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        type, HoldingSpaceFile(file_path, file_system_type, file_system_url),
        base::BindOnce(
            &holding_space_util::ResolveImage,
            primary_holding_space_service->thumbnail_loader_for_testing()));

    persisted_holding_space_items.Append(holding_space_item->Serialize());
    primary_holding_space_model->AddItem(std::move(holding_space_item));

    EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }

  // Verify persistent storage is updated when removing each type of item.
  while (!primary_holding_space_model->items().empty()) {
    const auto* holding_space_item =
        primary_holding_space_model->items()[0].get();

    persisted_holding_space_items.erase(persisted_holding_space_items.begin());
    primary_holding_space_model->RemoveItem(holding_space_item->id());

    EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
}

// Verifies that only finalized holding space items are persisted and that,
// once finalized, previously in progress holding space items are persisted at
// the appropriate index.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       PersistenceOfInProgressItems) {
  // Create a file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache the holding space model.
  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  HoldingSpaceModel* const holding_space_model =
      HoldingSpaceController::Get()->model();
  EXPECT_EQ(holding_space_model, holding_space_service->model_for_testing());

  // Initially, both the model and persistent storage should be empty.
  EXPECT_EQ(holding_space_model->items().size(), 0u);
  EXPECT_EQ(GetProfile()
                ->GetPrefs()
                ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                .size(),
            0u);

  // Add a finalized item to holding space. Because the item is finalized, it
  // should immediately be added to persistent storage.
  base::FilePath file_path = downloads_mount->CreateArbitraryFile();
  GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);
  HoldingSpaceFile::FileSystemType file_system_type =
      holding_space_util::ResolveFileSystemType(GetProfile(), file_system_url);
  auto finalized_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload,
      HoldingSpaceFile(file_path, file_system_type, file_system_url),
      base::BindOnce(&holding_space_util::ResolveImage,
                     holding_space_service->thumbnail_loader_for_testing()));
  auto* finalized_holding_space_item_ptr = finalized_holding_space_item.get();
  holding_space_model->AddItem(std::move(finalized_holding_space_item));

  base::Value::List persisted_holding_space_items;
  persisted_holding_space_items.Append(
      finalized_holding_space_item_ptr->Serialize());

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Add an in-progress item to holding space. Because the item is in progress,
  // it should *not* be added to persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  file_system_url = GetFileSystemUrl(GetProfile(), file_path);
  file_system_type =
      holding_space_util::ResolveFileSystemType(GetProfile(), file_system_url);
  auto in_progress_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload,
      HoldingSpaceFile(file_path, file_system_type,
                       GetFileSystemUrl(GetProfile(), file_path)),
      HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100),
      base::BindOnce(&holding_space_util::ResolveImage,
                     holding_space_service->thumbnail_loader_for_testing()));
  auto* in_progress_holding_space_item_ptr =
      in_progress_holding_space_item.get();
  holding_space_model->AddItem(std::move(in_progress_holding_space_item));

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Add another finalized item to holding space. Because the item is finalized,
  // it should immediately be added to persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  file_system_url = GetFileSystemUrl(GetProfile(), file_path);
  file_system_type =
      holding_space_util::ResolveFileSystemType(GetProfile(), file_system_url);
  finalized_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload,
      HoldingSpaceFile(file_path, file_system_type, file_system_url),
      base::BindOnce(&holding_space_util::ResolveImage,
                     holding_space_service->thumbnail_loader_for_testing()));
  finalized_holding_space_item_ptr = finalized_holding_space_item.get();
  holding_space_model->AddItem(std::move(finalized_holding_space_item));

  persisted_holding_space_items.Append(
      finalized_holding_space_item_ptr->Serialize());

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Update the file path for a finalized item. Because the item is finalized,
  // it should be updated immediately in persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  file_system_url = GetFileSystemUrl(GetProfile(), file_path);
  file_system_type =
      holding_space_util::ResolveFileSystemType(GetProfile(), file_system_url);
  holding_space_model->UpdateItem(finalized_holding_space_item_ptr->id())
      ->SetBackingFile(
          HoldingSpaceFile(file_path, file_system_type, file_system_url));

  ASSERT_EQ(persisted_holding_space_items.size(), 2u);
  persisted_holding_space_items[1u] =
      base::Value(finalized_holding_space_item_ptr->Serialize());

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Update the file path for the in-progress item. Because the item is still in
  // progress, it should not be added/updated to/in persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  file_system_url = GetFileSystemUrl(GetProfile(), file_path);
  file_system_type =
      holding_space_util::ResolveFileSystemType(GetProfile(), file_system_url);
  holding_space_model->UpdateItem(in_progress_holding_space_item_ptr->id())
      ->SetBackingFile(
          HoldingSpaceFile(file_path, file_system_type, file_system_url));

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Update the progress for the in-progress item. Because the item is still in
  // progress it should not be added/updated to/in persistent storage.
  holding_space_model->UpdateItem(in_progress_holding_space_item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100));

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Mark the in-progress item as finalized. Because the item is finalized, it
  // should be added to persistent storage at the appropriate index.
  holding_space_model->UpdateItem(in_progress_holding_space_item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100));

  ASSERT_EQ(persisted_holding_space_items.size(), 2u);
  persisted_holding_space_items.Insert(
      persisted_holding_space_items.begin() + 1u,
      base::Value(in_progress_holding_space_item_ptr->Serialize()));

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);
}

// Verifies that when a file backing a holding space item is moved, the holding
// space item is updated in place and persistence storage is updated.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       UpdatePersistentStorageAfterMove) {
  // Create a file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache the holding space model for the primary profile.
  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  HoldingSpaceModel* const primary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_EQ(primary_holding_space_model,
            primary_holding_space_service->model_for_testing());

  // Cache the file system context.
  storage::FileSystemContext* context =
      file_manager::util::GetFileManagerFileSystemContext(GetProfile());
  ASSERT_TRUE(context);

  base::Value::List persisted_holding_space_items;

  // Verify persistent storage is updated when adding each type of item.
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    // Note that each item is being added to a unique parent directory so that
    // moving the parent directory later will not affect other items.
    const base::FilePath file_path = downloads_mount->CreateFile(
        base::FilePath(base::NumberToString(static_cast<int>(type)))
            .Append("foo.txt"),
        /*content=*/std::string());
    const GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);
    const HoldingSpaceFile::FileSystemType file_system_type =
        holding_space_util::ResolveFileSystemType(GetProfile(),
                                                  file_system_url);

    // Create the holding space item.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        type, HoldingSpaceFile(file_path, file_system_type, file_system_url),
        base::BindOnce(
            &holding_space_util::ResolveImage,
            primary_holding_space_service->thumbnail_loader_for_testing()));

    // Add the holding space item to the model and verify persistence.
    persisted_holding_space_items.Append(holding_space_item->Serialize());
    primary_holding_space_model->AddItem(std::move(holding_space_item));
    EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }

  // Verify persistent storage is updated when moving each type of item and
  // that the holding space items themselves are updated in place.
  for (size_t i = 0; i < primary_holding_space_model->items().size(); ++i) {
    const auto* holding_space_item =
        primary_holding_space_model->items()[i].get();

    // Rename the file backing the holding space item.
    base::FilePath file_path = holding_space_item->file().file_path;
    base::FilePath new_file_path = file_path.InsertBeforeExtension(" (Moved)");
    GURL file_path_url = GetFileSystemUrl(GetProfile(), file_path);
    GURL new_file_path_url = GetFileSystemUrl(GetProfile(), new_file_path);
    {
      ItemUpdatedWaiter waiter(primary_holding_space_model, holding_space_item);
      ASSERT_EQ(
          storage::AsyncFileTestHelper::Move(
              context, context->CrackURLInFirstPartyContext(file_path_url),
              context->CrackURLInFirstPartyContext(new_file_path_url)),
          base::File::FILE_OK);

      // File changes must be posted to the UI thread, wait for the update to
      // reach the holding space model.
      waiter.Wait();
    }

    // Verify that the holding space item has been updated in place.
    ASSERT_EQ(holding_space_item->file().file_path, new_file_path);
    ASSERT_EQ(holding_space_item->file().file_system_url, new_file_path_url);
    ASSERT_EQ(holding_space_item->GetText(),
              new_file_path.BaseName().LossyDisplayName());

    // Verify that persistence has been updated.
    persisted_holding_space_items[i] =
        base::Value(holding_space_item->Serialize());
    ASSERT_EQ(GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);

    // Cache the base name of the file backing the holding space item as it will
    // not change due to rename of the holding space item's parent directory.
    base::FilePath base_name = holding_space_item->file().file_path.BaseName();

    // Rename the file backing the holding space item's parent directory.
    file_path = new_file_path.DirName();
    new_file_path = file_path.InsertBeforeExtension(" (Moved)");
    file_path_url = GetFileSystemUrl(GetProfile(), file_path);
    new_file_path_url = GetFileSystemUrl(GetProfile(), new_file_path);
    {
      ItemUpdatedWaiter waiter(primary_holding_space_model, holding_space_item);
      ASSERT_EQ(
          storage::AsyncFileTestHelper::Move(
              context, context->CrackURLInFirstPartyContext(file_path_url),
              context->CrackURLInFirstPartyContext(new_file_path_url)),
          base::File::FILE_OK);

      // File changes must be posted to the UI thread, wait for the update to
      // reach the holding space model.
      waiter.Wait();
    }

    // The file backing the holding space item is expected to have re-parented.
    new_file_path = new_file_path.Append(base_name);
    new_file_path_url = GetFileSystemUrl(GetProfile(), new_file_path);

    // Verify that the holding space item has been updated in place.
    ASSERT_EQ(holding_space_item->file().file_path, new_file_path);
    ASSERT_EQ(holding_space_item->file().file_system_url, new_file_path_url);
    ASSERT_EQ(holding_space_item->GetText(),
              new_file_path.BaseName().LossyDisplayName());

    // Verify that persistence has been updated.
    persisted_holding_space_items[i] =
        base::Value(holding_space_item->Serialize());
    ASSERT_EQ(GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
}

// Verifies that files that are trashed via the `TrashIOTask` are removed from
// the holding space model.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       TrashedFilesAreRemovedFromTheModel) {
  // Create a file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  // Ensure that required trash folders exist for the `downloads_mount`.
  const base::FilePath trash_path = downloads_mount->GetRootPath().Append(
      file_manager::trash::kTrashFolderName);
  ASSERT_TRUE(base::CreateDirectory(
      trash_path.Append(file_manager::trash::kFilesFolderName)));
  ASSERT_TRUE(base::CreateDirectory(
      trash_path.Append(file_manager::trash::kInfoFolderName)));

  // Cache the holding space model for the primary profile.
  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  HoldingSpaceModel* const primary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_EQ(primary_holding_space_model,
            primary_holding_space_service->model_for_testing());

  // Add each item to the holding space model.
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    const base::FilePath file_path = downloads_mount->CreateFile(
        base::FilePath(base::NumberToString(static_cast<int>(type)))
            .Append("foo.txt"),
        /*content=*/std::string());
    const GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);
    const HoldingSpaceFile::FileSystemType file_system_type =
        holding_space_util::ResolveFileSystemType(GetProfile(),
                                                  file_system_url);

    // Create the holding space item.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        type, HoldingSpaceFile(file_path, file_system_type, file_system_url),
        base::BindOnce(
            &holding_space_util::ResolveImage,
            primary_holding_space_service->thumbnail_loader_for_testing()));

    // Add the holding space item to the model.
    primary_holding_space_model->AddItem(std::move(holding_space_item));
  }

  // Use the File Manager's context for testing. Note that we specifically do
  // not use a test context since we want a production context which uses file
  // system operations that notify the `FileChangeService` on completion.
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(GetProfile());
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");

  // Keep sending the items in the model to the trash as each "trash" operation
  // should remove the item from the model.
  while (!primary_holding_space_model->items().empty()) {
    const auto* holding_space_item =
        primary_holding_space_model->items()[0].get();
    base::FilePath file_path = holding_space_item->file().file_path;

    ItemRemovedWaiter waiter(primary_holding_space_model, holding_space_item);

    base::test::TestFuture<file_manager::io_task::ProgressStatus> status;
    file_manager::io_task::TrashIOTask task(
        {file_system_context->CrackURLInFirstPartyContext(
            GetFileSystemUrl(GetProfile(), file_path))},
        GetProfile(), file_system_context, /*base_path=*/base::FilePath());
    task.Execute(base::DoNothing(), status.GetCallback());
    EXPECT_EQ(status.Get().state, file_manager::io_task::State::kSuccess);

    waiter.Wait();
  }

  // After trashing all the items (they now reside in .Trash/files/foo.txt) they
  // should not be visible in the holding space model.
  ASSERT_EQ(primary_holding_space_model->items().size(), 0u);
}

// Tests that holding space item's image representation gets updated when the
// backing file is changed using move operation. Furthermore, verifies that
// conflicts caused by moving a holding space item file to another path present
// in the holding space get resolved.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       UpdateItemsOverwrittenByMove) {
  // Create a file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache the holding space model for the primary profile.
  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  HoldingSpaceModel* const primary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_EQ(primary_holding_space_model,
            primary_holding_space_service->model_for_testing());

  // Cache the file system context.
  storage::FileSystemContext* context =
      file_manager::util::GetFileManagerFileSystemContext(GetProfile());
  ASSERT_TRUE(context);

  struct ItemInfo {
    std::string item_id;
    base::FilePath path;
    GURL file_system_url;
    HoldingSpaceFile::FileSystemType file_system_type;
  };
  struct TestCase {
    ItemInfo src;
    ItemInfo dst;
  };
  std::map<HoldingSpaceItem::Type, TestCase> test_config;

  base::Value::List persisted_holding_space_items;

  // Configure holding space state for the test. For each item adds two holding
  // space items to the model - "src" and "dst" (during the test, the src item's
  // file will be moved to the dst item's path).
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    auto add_item = [&](const std::string& file_name, ItemInfo* info) {
      info->path = downloads_mount->CreateFile(
          base::FilePath(base::NumberToString(static_cast<int>(type)))
              .Append(file_name),
          /*content=*/std::string());
      info->file_system_url = GetFileSystemUrl(GetProfile(), info->path);
      info->file_system_type = holding_space_util::ResolveFileSystemType(
          GetProfile(), info->file_system_url);

      // Create the holding space item.
      auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
          type,
          HoldingSpaceFile(info->path, info->file_system_type,
                           info->file_system_url),
          base::BindOnce(
              &holding_space_util::ResolveImage,
              primary_holding_space_service->thumbnail_loader_for_testing()));
      info->item_id = holding_space_item->id();

      // Add the holding space item to the model and verify persistence.
      persisted_holding_space_items.Append(holding_space_item->Serialize());
      primary_holding_space_model->AddItem(std::move(holding_space_item));
    };

    TestCase& test_case = test_config[type];
    add_item("src.txt", &test_case.src);
    add_item("dst.txt", &test_case.dst);

    ASSERT_NE(test_case.src.item_id, test_case.dst.item_id);
  }

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  base::Value::List final_persisted_holding_space_items;
  // Runs the test logic.
  for (const auto type : holding_space_util::GetAllItemTypes()) {
    const TestCase& test_case = test_config[type];

    const HoldingSpaceItem* src_item =
        primary_holding_space_model->GetItem(test_case.src.item_id);
    ASSERT_TRUE(src_item);

    // Move a file that was not in the holding space to the src path. Verify the
    // holding space item associated with this path remains in the holding space
    // in this case, and that its image representation gets updated.
    const base::FilePath path_not_in_holding_space =
        downloads_mount->CreateFile(
            base::FilePath(base::NumberToString(static_cast<int>(type)))
                .Append("not_in_holding_space.txt"),
            /*content=*/std::string());

    ItemImageUpdateWaiter image_update_waiter(src_item);
    ASSERT_EQ(storage::AsyncFileTestHelper::Move(
                  context,
                  context->CrackURLInFirstPartyContext(GetFileSystemUrl(
                      GetProfile(), path_not_in_holding_space)),
                  context->CrackURLInFirstPartyContext(
                      src_item->file().file_system_url)),
              base::File::FILE_OK);

    image_update_waiter.Wait();

    ASSERT_EQ(src_item,
              primary_holding_space_model->GetItem(test_case.src.item_id));
    EXPECT_TRUE(primary_holding_space_model->GetItem(test_case.dst.item_id));

    ASSERT_EQ(src_item->file().file_path, test_case.src.path);
    ASSERT_EQ(src_item->file().file_system_url, test_case.src.file_system_url);
    ASSERT_EQ(src_item->file().file_system_type,
              test_case.src.file_system_type);

    {
      ItemUpdatedWaiter waiter(primary_holding_space_model, src_item);
      // Move the file at the source item path to the destination item path.
      // Verify that, given that both paths are represented in the holding
      // space, the item initially associated with the destination path is
      // removed from the holding space (to avoid two items with the same
      // backing file).
      ASSERT_EQ(storage::AsyncFileTestHelper::Move(
                    context,
                    context->CrackURLInFirstPartyContext(
                        test_case.src.file_system_url),
                    context->CrackURLInFirstPartyContext(
                        test_case.dst.file_system_url)),
                base::File::FILE_OK);

      // File changes must be posted to the UI thread, wait for the update to
      // reach the holding space model.
      waiter.Wait();
    }

    const HoldingSpaceItem* item =
        primary_holding_space_model->GetItem(test_case.src.item_id);
    ASSERT_EQ(src_item,
              primary_holding_space_model->GetItem(test_case.src.item_id));
    EXPECT_FALSE(primary_holding_space_model->GetItem(test_case.dst.item_id));

    // Verify that the holding space item has been updated in place.
    ASSERT_EQ(src_item->file().file_path, test_case.dst.path);
    ASSERT_EQ(src_item->file().file_system_url, test_case.dst.file_system_url);
    ASSERT_EQ(src_item->file().file_system_type,
              test_case.dst.file_system_type);

    final_persisted_holding_space_items.Append(item->Serialize());
  }

  EXPECT_EQ(GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            final_persisted_holding_space_items);
}

// Verifies that the holding space model is restored from persistence. Note that
// when restoring from persistence, existence of backing files is verified and
// any stale holding space items are removed.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       RestorePersistentStorage) {
  // Verify expected histograms.
  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(kTotalCountV2HistogramPrefix),
      IsEmpty());

  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  // Verify `expected_histograms` after "waiting" for metrics debounce.
  task_environment()->FastForwardBy(base::Seconds(30));
  auto expected_histograms = GetExpectedTotalCountV2HistogramSamples(
      primary_holding_space_service->model_for_testing());
  for (const auto& [name, expected_buckets] : expected_histograms) {
    EXPECT_THAT(histogram_tester.GetAllSamples(name),
                BucketsAreArray(expected_buckets));
  }

  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value::List persisted_holding_space_items_after_restoration;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_holding_space_items_before_restoration;

        // Persist some holding space items of each type.
        for (const auto type : holding_space_util::GetAllItemTypes()) {
          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          const HoldingSpaceFile::FileSystemType file_system_type =
              holding_space_util::ResolveFileSystemType(GetProfile(),
                                                        file_system_url);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(file, file_system_type, file_system_url),
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          if (ShouldRestoreFromPersistence(type)) {
            // We expect the `fresh_holding_space_item` to still be in
            // persistence after model restoration since its backing file
            // exists.
            persisted_holding_space_items_after_restoration.Append(
                fresh_holding_space_item->Serialize());

            // We expect the `fresh_holding_space_item` to be restored from
            // persistence since its backing file exists.
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }

          base::FilePath file_path = downloads_mount->GetRootPath().AppendASCII(
              base::UnguessableToken::Create().ToString());
          auto stale_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(file_path,
                                   HoldingSpaceFile::FileSystemType::kTest,
                                   GURL("filesystem:fake_file_system_url")),
                  base::BindOnce(&CreateTestHoldingSpaceImage));

          // NOTE: While the `stale_holding_space_item` is persisted here, we do
          // *not* expect it to be restored or to be persisted after model
          // restoration since its backing file does *not* exist.
          persisted_holding_space_items_before_restoration.Append(
              stale_holding_space_item->Serialize());
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(
                std::move(persisted_holding_space_items_before_restoration)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile);
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  ItemsInitializedWaiter(secondary_holding_space_model).Wait();
  ASSERT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    EXPECT_EQ(*item, *restored_item)
        << "Expected equality of values at index " << i << ":"
        << "\n\tActual: " << item->id()
        << "\n\rRestored: " << restored_item->id();
  }

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);

  // Verify expected histograms after "waiting" for metrics debounce.
  // NOTE: Histograms are profile-agnostic and cumulative so we need to merge
  // `expected_histograms` from the primary profile with those of the secondary.
  task_environment()->FastForwardBy(base::Seconds(30));
  expected_histograms = MergeHistogramSamples(
      expected_histograms,
      GetExpectedTotalCountV2HistogramSamples(secondary_holding_space_model));
  for (const auto& [name, expected_buckets] : expected_histograms) {
    EXPECT_THAT(histogram_tester.GetAllSamples(name),
                BucketsAreArray(expected_buckets));
  }
}

// Verifies that items from volumes that are not immediately mounted during
// startup get restored into the holding space.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       RestorePersistentStorageForDelayedVolumeMount) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  auto delayed_mount = std::make_unique<ScopedTestMountPoint>(
      "drivefs-delayed_mount", storage::kFileSystemTypeDriveFs,
      file_manager::VOLUME_TYPE_GOOGLE_DRIVE);
  base::FilePath delayed_mount_file_name = base::FilePath("delayed file");

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  std::vector<std::string> initialized_items_before_delayed_mount;
  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value::List persisted_holding_space_items_after_restoration;
  base::Value::List persisted_holding_space_items_after_delayed_mount;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_holding_space_items_before_restoration;

        // Persist some holding space items of each type.
        for (const auto type : holding_space_util::GetAllItemTypes()) {
          const base::FilePath delayed_mount_file =
              delayed_mount->GetRootPath().Append(delayed_mount_file_name);
          auto delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(delayed_mount_file,
                                   HoldingSpaceFile::FileSystemType::kTest,
                                   GURL("filesystem:fake")),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          persisted_holding_space_items_before_restoration.Append(
              delayed_holding_space_item->Serialize());

          const bool should_restore = ShouldRestoreFromPersistence(type);

          // If an item should be restored, it should be restored after delayed
          // volume mount, and remain in persistent storage.
          if (should_restore) {
            persisted_holding_space_items_after_restoration.Append(
                delayed_holding_space_item->Serialize());
            persisted_holding_space_items_after_delayed_mount.Append(
                delayed_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(delayed_holding_space_item));
          }

          const base::FilePath non_existent_path =
              delayed_mount->GetRootPath().Append("non-existent");
          auto non_existant_delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(non_existent_path,
                                   HoldingSpaceFile::FileSystemType::kTest,
                                   GURL("filesystem:fake")),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          // The item should be removed from the model and persistent storage
          // after delayed volume mount (when it can be confirmed the backing
          // file does not exist) - the item should remain in persistent storage
          // until the associated volume is mounted.
          persisted_holding_space_items_before_restoration.Append(
              non_existant_delayed_holding_space_item->Serialize());

          if (should_restore) {
            persisted_holding_space_items_after_restoration.Append(
                non_existant_delayed_holding_space_item->Serialize());
          }

          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          const HoldingSpaceFile::FileSystemType file_system_type =
              holding_space_util::ResolveFileSystemType(GetProfile(),
                                                        file_system_url);
          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(file, file_system_type, file_system_url),
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));
          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          // The item should be immediately added to the model, and remain in
          // the persistent storage if it should be restored.
          if (should_restore) {
            initialized_items_before_delayed_mount.push_back(
                fresh_holding_space_item->id());
            persisted_holding_space_items_after_restoration.Append(
                fresh_holding_space_item->Serialize());
            persisted_holding_space_items_after_delayed_mount.Append(
                fresh_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(
                std::move(persisted_holding_space_items_before_restoration)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile);
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();

  EXPECT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  ItemsInitializedWaiter(secondary_holding_space_model)
      .Wait(
          /*filter=*/base::BindLambdaForTesting(
              [&downloads_mount](const HoldingSpaceItem* item) -> bool {
                return downloads_mount->GetRootPath().IsParent(
                    item->file().file_path);
              }));

  std::vector<std::string> initialized_items;
  for (const auto& item : secondary_holding_space_model->items()) {
    if (item->IsInitialized()) {
      initialized_items.push_back(item->id());
    }
  }
  EXPECT_EQ(initialized_items_before_delayed_mount, initialized_items);

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);

  delayed_mount->CreateFile(delayed_mount_file_name, "fake");
  delayed_mount->Mount(secondary_profile);

  ItemsInitializedWaiter(secondary_holding_space_model).Wait();

  EXPECT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    SCOPED_TRACE(testing::Message() << "Item at index " << i);

    EXPECT_TRUE(item->IsInitialized());

    EXPECT_EQ(item->id(), restored_item->id());
    EXPECT_EQ(item->type(), restored_item->type());
    EXPECT_EQ(item->GetText(), restored_item->GetText());
    EXPECT_EQ(item->file().file_path, restored_item->file().file_path);
    // NOTE: `restored_item` was created with a fake file system URL (as it
    // could not be properly resolved at the time of item creation).
    EXPECT_EQ(
        item->file().file_system_url,
        GetFileSystemUrl(secondary_profile, restored_item->file().file_path));
  }

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_delayed_mount);
}

// Verifies that items from volumes that are not immediately mounted during
// startup get restored into the holding space - same as
// RestorePersistentStorageForDelayedVolumeMount, but the volume gets mounted
// while item restoration is in progress.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       RestorePersistentStorageForDelayedVolumeMountDuringRestoration) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  auto delayed_mount = std::make_unique<ScopedTestMountPoint>(
      "drivefs-delayed_mount", storage::kFileSystemTypeDriveFs,
      file_manager::VOLUME_TYPE_GOOGLE_DRIVE);
  base::FilePath delayed_mount_file_name = base::FilePath("delayed file");

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value::List persisted_holding_space_items_after_delayed_mount;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_holding_space_items_before_restoration;

        // Persist some holding space items of each type.
        for (const auto type : holding_space_util::GetAllItemTypes()) {
          const base::FilePath delayed_mount_file =
              delayed_mount->GetRootPath().Append(delayed_mount_file_name);
          auto delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(delayed_mount_file,
                                   HoldingSpaceFile::FileSystemType::kTest,
                                   GURL("filesystem:fake")),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          persisted_holding_space_items_before_restoration.Append(
              delayed_holding_space_item->Serialize());

          const bool should_restore = ShouldRestoreFromPersistence(type);

          // The item is restored after delayed volume mount, and remain
          // in persistent storage if it should be restored.
          if (should_restore) {
            persisted_holding_space_items_after_delayed_mount.Append(
                delayed_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(delayed_holding_space_item));
          }

          base::FilePath non_existent_path =
              delayed_mount->GetRootPath().Append("non-existent");
          auto non_existant_delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(non_existent_path,
                                   HoldingSpaceFile::FileSystemType::kTest,
                                   GURL("filesystem:fake")),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          // The item should be removed from the model and persistent storage
          // after delayed volume mount (when it can be confirmed the backing
          // file does not exist) - the item should remain in persistent storage
          // until the associated volume is mounted.
          persisted_holding_space_items_before_restoration.Append(
              non_existant_delayed_holding_space_item->Serialize());

          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          const HoldingSpaceFile::FileSystemType file_system_type =
              holding_space_util::ResolveFileSystemType(GetProfile(),
                                                        file_system_url);
          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(file, file_system_type, file_system_url),
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          // The item should be immediately added to the model, and remain in
          // the persistent storage if it should be restored.
          if (should_restore) {
            persisted_holding_space_items_after_delayed_mount.Append(
                fresh_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(
                std::move(persisted_holding_space_items_before_restoration)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  ActivateSecondaryProfile();

  delayed_mount->CreateFile(delayed_mount_file_name, "fake");
  delayed_mount->Mount(secondary_profile);

  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile);
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();

  EXPECT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  ItemsInitializedWaiter(secondary_holding_space_model).Wait();
  ASSERT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    SCOPED_TRACE(testing::Message() << "Item at index " << i);

    EXPECT_TRUE(item->IsInitialized());

    EXPECT_EQ(item->id(), restored_item->id());
    EXPECT_EQ(item->type(), restored_item->type());
    EXPECT_EQ(item->GetText(), restored_item->GetText());
    EXPECT_EQ(item->file().file_path, restored_item->file().file_path);
    // NOTE: `restored_item` was created with a fake file system URL (as it
    // could not be properly resolved at the time of item creation).
    EXPECT_EQ(
        item->file().file_system_url,
        GetFileSystemUrl(secondary_profile, restored_item->file().file_path));
  }

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_delayed_mount);
}

// Verifies that mounting volumes that contain no holding space items does not
// interfere with holding space restoration.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       RestorePersistentStorageWithUnrelatedVolumeMounts) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  auto delayed_mount_1 = std::make_unique<ScopedTestMountPoint>(
      "drivefs-delayed_mount_1", storage::kFileSystemTypeDriveFs,
      file_manager::VOLUME_TYPE_GOOGLE_DRIVE);

  auto delayed_mount_2 = std::make_unique<ScopedTestMountPoint>(
      "drivefs-delayed_mount_2", storage::kFileSystemTypeDriveFs,
      file_manager::VOLUME_TYPE_GOOGLE_DRIVE);

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  std::vector<std::string> initialized_items_before_delayed_mount;
  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value::List persisted_holding_space_items_after_restoration;
  base::Value::List persisted_holding_space_items_after_delayed_mount;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_holding_space_items_before_restoration;

        // Persist some holding space items of each type.
        for (const auto type : holding_space_util::GetAllItemTypes()) {
          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          const HoldingSpaceFile::FileSystemType file_system_type =
              holding_space_util::ResolveFileSystemType(GetProfile(),
                                                        file_system_url);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(file, file_system_type, file_system_url),
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          // The item should be immediately added to the model, and remain in
          // the persistent storage if it should be restored.
          if (ShouldRestoreFromPersistence(type)) {
            initialized_items_before_delayed_mount.push_back(
                fresh_holding_space_item->id());
            persisted_holding_space_items_after_restoration.Append(
                fresh_holding_space_item->Serialize());
            persisted_holding_space_items_after_delayed_mount.Append(
                fresh_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(
                std::move(persisted_holding_space_items_before_restoration)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  ActivateSecondaryProfile();
  delayed_mount_1->Mount(secondary_profile);
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile);
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();

  EXPECT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  ItemsInitializedWaiter(secondary_holding_space_model).Wait();

  std::vector<std::string> initialized_items;
  for (const auto& item : secondary_holding_space_model->items()) {
    if (item->IsInitialized()) {
      initialized_items.push_back(item->id());
    }
  }
  EXPECT_EQ(initialized_items_before_delayed_mount, initialized_items);

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);

  delayed_mount_2->Mount(secondary_profile);
  ItemsInitializedWaiter(secondary_holding_space_model).Wait();

  EXPECT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    SCOPED_TRACE(testing::Message() << "Item at index " << i);

    EXPECT_TRUE(item->IsInitialized());

    EXPECT_EQ(item->id(), restored_item->id());
    EXPECT_EQ(item->type(), restored_item->type());
    EXPECT_EQ(item->GetText(), restored_item->GetText());
    EXPECT_EQ(item->file().file_path, restored_item->file().file_path);
    // NOTE: `restored_item` was created with a fake file system URL (as it
    // could not be properly resolved at the time of item creation).
    EXPECT_EQ(
        item->file().file_system_url,
        GetFileSystemUrl(secondary_profile, restored_item->file().file_path));
  }

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_delayed_mount);
}

// Tests that items from an unmounted volume get removed from the holding space.
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       RemoveItemsFromUnmountedVolumes) {
  auto test_mount_1 = std::make_unique<ScopedTestMountPoint>(
      "test_mount_1", storage::kFileSystemTypeLocal,
      file_manager::VOLUME_TYPE_TESTING);
  test_mount_1->Mount(GetProfile());
  HoldingSpaceModelAttachedWaiter(GetProfile()).Wait();

  auto test_mount_2 = std::make_unique<ScopedTestMountPoint>(
      "test_mount_2", storage::kFileSystemTypeLocal,
      file_manager::VOLUME_TYPE_TESTING);
  test_mount_2->Mount(GetProfile());
  HoldingSpaceModelAttachedWaiter(GetProfile()).Wait();
  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  const HoldingSpaceModel* holding_space_model =
      holding_space_service->model_for_testing();

  const base::FilePath file_path_1 = test_mount_1->CreateArbitraryFile();
  holding_space_service->AddItemOfType(HoldingSpaceItem::Type::kScreenshot,
                                       file_path_1);

  const base::FilePath file_path_2 = test_mount_2->CreateArbitraryFile();
  holding_space_service->AddItemOfType(HoldingSpaceItem::Type::kDownload,
                                       file_path_2);

  const base::FilePath file_path_3 = test_mount_1->CreateArbitraryFile();
  holding_space_service->AddItemOfType(HoldingSpaceItem::Type::kDownload,
                                       file_path_3);

  EXPECT_EQ(3u, GetProfile()
                    ->GetPrefs()
                    ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                    .size());
  EXPECT_EQ(3u, holding_space_model->items().size());

  test_mount_1.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, GetProfile()
                    ->GetPrefs()
                    ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                    .size());
  ASSERT_EQ(1u, holding_space_model->items().size());
  EXPECT_EQ(file_path_2, holding_space_model->items()[0]->file().file_path);
}

// Verifies that files restored from persistence are not older than
// `kMaxFileAge`.
// TODO(crbug.com/1427927): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_RemoveOlderFilesFromPersistence \
  DISABLED_RemoveOlderFilesFromPersistence
#else
#define MAYBE_RemoveOlderFilesFromPersistence RemoveOlderFilesFromPersistence
#endif
TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       MAYBE_RemoveOlderFilesFromPersistence) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value::List persisted_holding_space_items_after_restoration;
  base::Time last_creation_time = base::Time::Now();

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_holding_space_items_before_restoration;

        // Persist some holding space items of each type.
        for (const auto type : holding_space_util::GetAllItemTypes()) {
          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          const HoldingSpaceFile::FileSystemType file_system_type =
              holding_space_util::ResolveFileSystemType(GetProfile(),
                                                        file_system_url);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  HoldingSpaceFile(file, file_system_type, file_system_url),
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          bool should_restore = ShouldRestoreFromPersistence(type);

          if (should_restore) {
            // We expect all holding space items of other types to be removed
            // from persistence during restoration due to being older than
            // `kMaxFileAge`.
            should_restore = type == HoldingSpaceItem::Type::kPinnedFile;
          }

          if (should_restore) {
            persisted_holding_space_items_after_restoration.Append(
                fresh_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }

          base::File::Info file_info;
          ASSERT_TRUE(base::GetFileInfo(file, &file_info));
          last_creation_time = file_info.creation_time;
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(
                std::move(persisted_holding_space_items_before_restoration)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  // Fast-forward to a point where the created files are too old to be restored
  // from persistence.
  task_environment()->FastForwardBy(last_creation_time - base::Time::Now() +
                                    kMaxFileAge);

  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile);
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  ItemsInitializedWaiter(secondary_holding_space_model).Wait();

  ASSERT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    EXPECT_EQ(*item, *restored_item)
        << "Expected equality of values at index " << i << ":"
        << "\n\tActual: " << item->id()
        << "\n\rRestored: " << restored_item->id();
  }

  // Verify persisted holding space items.
  EXPECT_EQ(secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       AddArcDownloadItem) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space `model` is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());

  // Create a test downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Create a fake download file on the local file system.
  const base::FilePath file_path = downloads_mount->CreateFile(
      /*relative_path=*/base::FilePath("Download.png"),
      /*content=*/"foo");

  // Simulate an `OnMediaStoreUriAdded()` event from ARC.
  auto* arc_file_system_bridge =
      arc::ArcFileSystemBridge::GetForBrowserContext(profile);
  ASSERT_TRUE(arc_file_system_bridge);
  arc_file_system_bridge->OnMediaStoreUriAdded(
      GURL("uri"), arc::mojom::MediaStoreMetadata::NewDownload(
                       arc::mojom::MediaStoreDownloadMetadata::New(
                           /*display_name=*/file_path.BaseName().value(),
                           /*owner_package_name=*/"com.bar.foo",
                           /*relative_path=*/base::FilePath("Download/"))));

  // Verify that an item of type `kArcDownload` was added to holding space.
  ASSERT_EQ(1u, model->items().size());
  const HoldingSpaceItem* arc_download_item = model->items()[0].get();
  EXPECT_EQ(arc_download_item->type(), HoldingSpaceItem::Type::kArcDownload);
  EXPECT_EQ(arc_download_item->file().file_path,
            file_manager::util::GetDownloadsFolderForProfile(profile).Append(
                base::FilePath("Download.png")));
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       AddInProgressDownloadItem) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space model is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->items().size(), 0u);

  // Create a downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache current state, file paths, received bytes, and total bytes.
  auto current_state = download::DownloadItem::IN_PROGRESS;
  base::FilePath current_path;
  base::FilePath current_target_path;
  int64_t current_received_bytes = 0;
  int64_t current_total_bytes = 100;
  bool current_is_dangerous = false;
  download::DownloadDangerType current_danger_type =
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;

  // Create a fake download item and cache a function to update it.
  std::unique_ptr<content::FakeDownloadItem> fake_download_item =
      CreateFakeDownloadItem(profile, current_state, current_path,
                             current_target_path, current_received_bytes,
                             current_total_bytes);
  auto UpdateFakeDownloadItem = [&]() {
    fake_download_item->SetDummyFilePath(current_path);
    fake_download_item->SetReceivedBytes(current_received_bytes);
    fake_download_item->SetState(current_state);
    fake_download_item->SetTargetFilePath(current_target_path);
    fake_download_item->SetTotalBytes(current_total_bytes);
    fake_download_item->SetIsDangerous(current_is_dangerous);
    fake_download_item->SetDangerType(current_danger_type);
    fake_download_item->NotifyDownloadUpdated();
  };

  // Verify that no holding space item has been created since the download does
  // not yet have file path set.
  EXPECT_EQ(model->items().size(), 0u);

  // Update the file paths for the download.
  current_path = downloads_mount->CreateFile(base::FilePath("foo.crdownload"));
  current_target_path = downloads_mount->CreateFile(base::FilePath("foo.png"));
  UpdateFakeDownloadItem();

  // Verify that a holding space item has been created.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.f);

  constexpr gfx::Size kImageSize(20, 20);
  constexpr bool kDarkBackground = false;

  {
    // Once the `ThumbnailLoader` has finished processing the initial request,
    // the image should represent the file type of the *target* file for the
    // underlying download, not its current backing file.
    base::RunLoop run_loop;
    auto image_skia_changed_subscription =
        model->items()[0]->image().AddImageSkiaChangedCallback(
            base::BindLambdaForTesting([&]() {
              gfx::ImageSkia actual_image =
                  model->items()[0]->image().GetImageSkia(kImageSize,
                                                          kDarkBackground);
              gfx::ImageSkia expected_image = chromeos::GetIconForPath(
                  current_target_path, kDarkBackground);
              EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));
              run_loop.Quit();
            }));

    // But initially the holding space image should be an empty bitmap. Note
    // that requesting the image is what spawns the initial request.
    gfx::ImageSkia actual_image =
        model->items()[0]->image().GetImageSkia(kImageSize, kDarkBackground);
    gfx::ImageSkia expected_image = image_util::CreateEmptyImage(kImageSize);
    EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));

    // Wait for the `ThumbnailLoader` to finish processing the initial request.
    run_loop.Run();
  }

  // Update the total bytes for the download.
  current_total_bytes = -1;
  UpdateFakeDownloadItem();

  // Verify that the holding space item has indeterminate progress.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsIndeterminate());

  // Update the received bytes and total bytes for the download.
  current_received_bytes = 50;
  current_total_bytes = 100;
  UpdateFakeDownloadItem();

  // Verify that the holding space item has expected progress.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.5f);

  // Remove the holding space item from the model.
  model->RemoveIf(
      base::BindRepeating([](const HoldingSpaceItem* item) { return true; }));
  EXPECT_EQ(model->items().size(), 0u);

  // Complete the download.
  current_state = download::DownloadItem::COMPLETE;
  current_path = current_target_path;
  current_received_bytes = current_total_bytes;
  UpdateFakeDownloadItem();

  // Verify that no holding space item has been created since the holding space
  // associated with the completed download was previously removed.
  EXPECT_EQ(model->items().size(), 0u);

  // Create a new download.
  current_state = download::DownloadItem::IN_PROGRESS;
  current_path = base::FilePath();
  current_target_path = base::FilePath();
  current_received_bytes = 0;
  fake_download_item = CreateFakeDownloadItem(
      profile, current_state, current_path, current_target_path,
      current_received_bytes, current_total_bytes);

  // Verify that no holding space item has been created since the download does
  // not yet have file path set.
  EXPECT_EQ(model->items().size(), 0u);

  // Update the file paths and received bytes for the download.
  current_path = downloads_mount->CreateFile(base::FilePath("bar.crdownload"));
  current_target_path = downloads_mount->CreateFile(base::FilePath("bar.zip"));
  current_received_bytes = 50;
  UpdateFakeDownloadItem();

  // Verify that a holding space item has been created.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.5f);

  // Not dangerous in-progress items should only have Cancel and Pause
  // in-progress commands.
  EXPECT_EQ(model->items()[0]->in_progress_commands().size(), 2u);
  EXPECT_TRUE(holding_space_util::SupportsInProgressCommand(
      model->items()[0].get(), HoldingSpaceCommandId::kCancelItem));
  EXPECT_TRUE(holding_space_util::SupportsInProgressCommand(
      model->items()[0].get(), HoldingSpaceCommandId::kPauseItem));

  {
    // Once the `ThumbnailLoader` has finished processing the request, the image
    // should represent the file type of the *target* file for the underlying
    // download, not its current backing file.
    base::RunLoop run_loop;
    auto image_skia_changed_subscription =
        model->items()[0]->image().AddImageSkiaChangedCallback(
            base::BindLambdaForTesting([&]() {
              gfx::ImageSkia actual_image =
                  model->items()[0]->image().GetImageSkia(kImageSize,
                                                          kDarkBackground);
              gfx::ImageSkia expected_image = chromeos::GetIconForPath(
                  current_target_path, kDarkBackground);
              EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));
              run_loop.Quit();
            }));

    // But initially the holding space image should be an empty bitmap. Note
    // that requesting the image is what spawns the initial request.
    gfx::ImageSkia actual_image =
        model->items()[0]->image().GetImageSkia(kImageSize, kDarkBackground);
    gfx::ImageSkia expected_image = image_util::CreateEmptyImage(kImageSize);
    EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));

    // Wait for the `ThumbnailLoader` to finish processing the initial request.
    run_loop.Run();
  }

  // Mark the download as dangerous and maybe malicious.
  current_is_dangerous = true;
  current_danger_type = download::DownloadDangerType::
      DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
  UpdateFakeDownloadItem();

  // Dangerous in-progress items should only have Cancel in-progress commands.
  EXPECT_EQ(model->items()[0]->in_progress_commands().size(), 1u);
  EXPECT_TRUE(holding_space_util::SupportsInProgressCommand(
      model->items()[0].get(), HoldingSpaceCommandId::kCancelItem));

  {
    // Because the download has been marked as dangerous and maybe malicious,
    // the image should represent that the underlying download is in error.
    base::RunLoop run_loop;
    auto image_skia_changed_subscription =
        model->items()[0]->image().AddImageSkiaChangedCallback(
            base::BindLambdaForTesting([&]() {
              gfx::ImageSkia actual_image =
                  model->items()[0]->image().GetImageSkia(kImageSize,
                                                          kDarkBackground);
              gfx::ImageSkia expected_image =
                  gfx::ImageSkiaOperations::CreateSuperimposedImage(
                      image_util::CreateEmptyImage(kImageSize),
                      gfx::CreateVectorIcon(
                          vector_icons::kErrorOutlineIcon,
                          kHoldingSpaceIconSize,
                          cros_styles::ResolveColor(
                              cros_styles::ColorName::kIconColorAlert,
                              kDarkBackground)));
              EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));
              run_loop.Quit();
            }));

    // Force a thumbnail request and wait for the `ThumbnailLoader` to finish
    // processing the request.
    model->items()[0]->image().GetImageSkia(kImageSize, kDarkBackground);
    run_loop.Run();
  }

  // Mark the download as *not* being malicious.
  current_danger_type =
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  UpdateFakeDownloadItem();

  // Dangerous in-progress items should only have Cancel in-progress commands.
  EXPECT_EQ(model->items()[0]->in_progress_commands().size(), 1u);
  EXPECT_TRUE(holding_space_util::SupportsInProgressCommand(
      model->items()[0].get(), HoldingSpaceCommandId::kCancelItem));

  {
    // Because the download has been marked as dangerous but *not* malicious,
    // the image should represent that the underlying download is in warning.
    base::RunLoop run_loop;
    auto image_skia_changed_subscription =
        model->items()[0]->image().AddImageSkiaChangedCallback(
            base::BindLambdaForTesting([&]() {
              gfx::ImageSkia actual_image =
                  model->items()[0]->image().GetImageSkia(kImageSize,
                                                          kDarkBackground);
              gfx::ImageSkia expected_image =
                  gfx::ImageSkiaOperations::CreateSuperimposedImage(
                      image_util::CreateEmptyImage(kImageSize),
                      gfx::CreateVectorIcon(
                          vector_icons::kErrorOutlineIcon,
                          kHoldingSpaceIconSize,
                          cros_styles::ResolveColor(
                              cros_styles::ColorName::kIconColorWarning,
                              kDarkBackground)));
              EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));
              run_loop.Quit();
            }));

    // Force a thumbnail request and wait for the `ThumbnailLoader` to finish
    // processing the request.
    model->items()[0]->image().GetImageSkia(kImageSize, kDarkBackground);
    run_loop.Run();
  }

  // Complete the download.
  current_state = download::DownloadItem::COMPLETE;
  current_path = current_target_path;
  current_received_bytes = current_total_bytes;
  UpdateFakeDownloadItem();

  // Verify that the holding space item has been updated.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsComplete());

  // The image should be representative of the file type of the *target* file
  // for the underlying download which by this point is actually the same file
  // path as the backing file path.
  gfx::ImageSkia actual_image =
      model->items()[0]->image().GetImageSkia(kImageSize, kDarkBackground);
  gfx::ImageSkia expected_image =
      chromeos::GetIconForPath(current_target_path, kDarkBackground);
  EXPECT_TRUE(BitmapsAreEqual(actual_image, expected_image));
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest, RemoveAll) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space `model` is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());

  // Create a test mount point.
  std::unique_ptr<ScopedTestMountPoint> mount_point =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(mount_point->IsValid());

  auto* service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile);

  // Create files on the file system.
  const base::FilePath download_path = mount_point->CreateFile(
      /*relative_path=*/base::FilePath("bar"), /*content=*/"bar");
  const base::FilePath pinned_file_path = mount_point->CreateFile(
      /*relative_path=*/base::FilePath("foo"), /*content=*/"foo");

  // Add them both to holding space, one in pinned files the other in downloads.
  service->AddItemOfType(HoldingSpaceItem::Type::kDownload, download_path);
  service->AddPinnedFiles(
      {file_manager::util::GetFileManagerFileSystemContext(profile)
           ->CrackURLInFirstPartyContext(
               holding_space_util::ResolveFileSystemUrl(profile,
                                                        pinned_file_path))},
      holding_space_metrics::EventSource::kTest);

  ASSERT_EQ(2u, model->items().size());
  service->RemoveAll();
  EXPECT_EQ(0u, model->items().size());
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       CreateInterruptedDownloadItem) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space model is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->items().size(), 0u);

  // Create a downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache current state, file paths, received bytes, and total bytes.
  auto current_state = download::DownloadItem::INTERRUPTED;
  base::FilePath current_path;
  base::FilePath current_target_path;
  int64_t current_received_bytes = 0;
  int64_t current_total_bytes = 100;
  bool current_is_dangerous = false;

  // Create a fake download item and cache a function to update it.
  std::unique_ptr<content::FakeDownloadItem> fake_download_item =
      CreateFakeDownloadItem(profile, current_state, current_path,
                             current_target_path, current_received_bytes,
                             current_total_bytes);
  auto UpdateFakeDownloadItem = [&]() {
    fake_download_item->SetDummyFilePath(current_path);
    fake_download_item->SetReceivedBytes(current_received_bytes);
    fake_download_item->SetState(current_state);
    fake_download_item->SetTargetFilePath(current_target_path);
    fake_download_item->SetTotalBytes(current_total_bytes);
    fake_download_item->SetIsDangerous(current_is_dangerous);
    fake_download_item->NotifyDownloadUpdated();
  };

  // Verify that no holding space item has been created since the download does
  // not yet have file path set.
  EXPECT_EQ(model->items().size(), 0u);

  // Update the file paths for the download.
  current_path = downloads_mount->CreateFile(base::FilePath("foo.crdownload"));
  current_target_path = downloads_mount->CreateFile(base::FilePath("foo.png"));
  UpdateFakeDownloadItem();

  // Verify that no holding space item has been created since the download is
  // not in progress yet.
  EXPECT_EQ(model->items().size(), 0u);

  current_state = download::DownloadItem::IN_PROGRESS;
  UpdateFakeDownloadItem();

  // Verify that a holding space item is created.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.f);

  // Complete the download.
  current_state = download::DownloadItem::COMPLETE;
  current_path = current_target_path;
  current_received_bytes = current_total_bytes;
  UpdateFakeDownloadItem();

  // Verify that completing a download results in exactly one holding space item
  // existing for it, regardless of whether the in-progress downloads feature is
  // enabled.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsComplete());
}

TEST_P(HoldingSpaceKeyedServiceWithExperimentalFeatureTest,
       InterruptAndResumeDownload) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space model is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->items().size(), 0u);

  // Create a downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache current state, file paths, received bytes, and total bytes.
  auto current_state = download::DownloadItem::IN_PROGRESS;
  base::FilePath current_path;
  base::FilePath current_target_path;
  int64_t current_received_bytes = 0;
  int64_t current_total_bytes = 100;
  bool current_is_dangerous = false;

  // Create a fake download item and cache a function to update it.
  std::unique_ptr<content::FakeDownloadItem> fake_download_item =
      CreateFakeDownloadItem(profile, current_state, current_path,
                             current_target_path, current_received_bytes,
                             current_total_bytes);
  auto UpdateFakeDownloadItem = [&]() {
    fake_download_item->SetDummyFilePath(current_path);
    fake_download_item->SetReceivedBytes(current_received_bytes);
    fake_download_item->SetState(current_state);
    fake_download_item->SetTargetFilePath(current_target_path);
    fake_download_item->SetTotalBytes(current_total_bytes);
    fake_download_item->SetIsDangerous(current_is_dangerous);
    fake_download_item->NotifyDownloadUpdated();
  };

  // Verify that no holding space item has been created since the download does
  // not yet have file path set.
  EXPECT_EQ(model->items().size(), 0u);

  // Update the file paths for the download.
  current_path = downloads_mount->CreateFile(base::FilePath("foo.crdownload"));
  current_target_path = downloads_mount->CreateFile(base::FilePath("foo.png"));
  UpdateFakeDownloadItem();

  // Verify that a holding space item is created.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.f);

  // Make some progress and interrupt the download.
  current_received_bytes = 50;
  current_state = download::DownloadItem::INTERRUPTED;
  UpdateFakeDownloadItem();

  // Verify that interrupting an in-progress download destroys its holding
  // space item (if the in-progress downloads feature is enabled).
  ASSERT_EQ(model->items().size(), 0u);

  // Resume the download.
  current_state = download::DownloadItem::IN_PROGRESS;
  UpdateFakeDownloadItem();

  // Verify that resuming an interrupted download creates a new holding space
  // item.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.5f);

  // Complete the download.
  current_state = download::DownloadItem::COMPLETE;
  current_path = current_target_path;
  current_received_bytes = current_total_bytes;
  UpdateFakeDownloadItem();

  // Verify that completing a download results in exactly one holding space item
  // existing for it, regardless of whether the in-progress downloads feature is
  // enabled.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file().file_path, current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsComplete());
}

// Base class for tests which verify adding and removing items from holding
// space works as intended, parameterized by holding space item type.
class HoldingSpaceKeyedServiceAddAndRemoveItemTest
    : public HoldingSpaceKeyedServiceTest,
      public ::testing::WithParamInterface<HoldingSpaceItem::Type> {
 public:
  // Returns the holding space service associated with the specified `profile`.
  HoldingSpaceKeyedService* GetService(Profile* profile) {
    return HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile);
  }

  // Returns the type of holding space item under test.
  HoldingSpaceItem::Type GetType() const { return GetParam(); }

  // Adds an item of `type` to the holding space belonging to `profile`, backed
  // by the file at the specified absolute `file_path`. Returns the `id` of the
  // added holding space item.
  const std::string& AddItem(Profile* profile,
                             HoldingSpaceItem::Type type,
                             const base::FilePath& file_path) {
    auto* const holding_space_service = GetService(profile);
    EXPECT_TRUE(holding_space_service);
    const auto* holding_space_model =
        holding_space_service->model_for_testing();
    EXPECT_TRUE(holding_space_model);

    switch (type) {
      case HoldingSpaceItem::Type::kArcDownload:
      case HoldingSpaceItem::Type::kDownload:
      case HoldingSpaceItem::Type::kLacrosDownload:
        EXPECT_EQ(
            holding_space_model->ContainsItem(type, file_path),
            holding_space_service->AddItemOfType(type, file_path).empty());
        break;
      case HoldingSpaceItem::Type::kDiagnosticsLog:
      case HoldingSpaceItem::Type::kNearbyShare:
        holding_space_service->AddItemOfType(type, file_path);
        break;
      case HoldingSpaceItem::Type::kDriveSuggestion:
      case HoldingSpaceItem::Type::kLocalSuggestion:
        holding_space_service->SetSuggestions(
            /*suggestions=*/{{type, file_path}});
        break;
      case HoldingSpaceItem::Type::kPinnedFile:
        holding_space_service->AddPinnedFiles(
            {file_manager::util::GetFileManagerFileSystemContext(profile)
                 ->CrackURLInFirstPartyContext(
                     holding_space_util::ResolveFileSystemUrl(profile,
                                                              file_path))},
            holding_space_metrics::EventSource::kTest);
        break;
      case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
        EXPECT_EQ(
            holding_space_model->ContainsItem(type, file_path),
            holding_space_service
                ->AddItemOfType(HoldingSpaceItem::Type::kPhoneHubCameraRoll,
                                file_path, HoldingSpaceProgress())
                .empty());
        break;
      case HoldingSpaceItem::Type::kPhotoshopWeb:
      case HoldingSpaceItem::Type::kPrintedPdf:
      case HoldingSpaceItem::Type::kScan:
      case HoldingSpaceItem::Type::kScreenRecording:
      case HoldingSpaceItem::Type::kScreenRecordingGif:
      case HoldingSpaceItem::Type::kScreenshot:
        holding_space_service->AddItemOfType(type, file_path);
        break;
    }

    const auto* item = holding_space_model->GetItem(type, file_path);
    EXPECT_TRUE(item);
    return item->id();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceKeyedServiceAddAndRemoveItemTest,
    testing::ValuesIn(holding_space_util::GetAllItemTypes()));

TEST_P(HoldingSpaceKeyedServiceAddAndRemoveItemTest, AddAndRemoveItem) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space `model` is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());

  // Verify expected histograms.
  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(kTotalCountV2HistogramPrefix),
      IsEmpty());

  // Create a test mount point.
  std::unique_ptr<ScopedTestMountPoint> mount_point =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(mount_point->IsValid());

  // Create a file on the file system.
  const base::FilePath file_path = mount_point->CreateFile(
      /*relative_path=*/base::FilePath("foo"), /*content=*/"foo");

  // Add a holding space item of the type under test.
  const std::string id = AddItem(profile, GetType(), file_path);

  // Verify a holding space item has been added to the model.
  ASSERT_EQ(model->items().size(), 1u);

  HoldingSpaceKeyedService* const service = GetService(profile);
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->ContainsItem(id));

  // Verify holding space `item` metadata.
  HoldingSpaceItem* const item = model->items()[0].get();
  EXPECT_EQ(item->id(), id);
  EXPECT_EQ(item->type(), GetType());
  EXPECT_EQ(item->GetText(), file_path.BaseName().LossyDisplayName());
  EXPECT_EQ(item->file().file_path, file_path);
  EXPECT_EQ(item->file().file_system_url,
            holding_space_util::ResolveFileSystemUrl(profile, file_path));

  // Verify holding space `item` image.
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           GetService(profile)->thumbnail_loader_for_testing(), GetType(),
           file_path)
           ->GetImageSkia()
           .bitmap(),
      *item->image().GetImageSkia().bitmap()));

  // Verify `expected_histograms` after "waiting" for metrics debounce.
  task_environment()->FastForwardBy(base::Seconds(30));
  auto expected_histograms = GetExpectedTotalCountV2HistogramSamples(model);
  for (const auto& [name, expected_buckets] : expected_histograms) {
    EXPECT_THAT(histogram_tester.GetAllSamples(name),
                BucketsAreArray(expected_buckets));
  }

  // Attempt to add a holding space item of the same type and `file_path`.
  const std::string& id2 = AddItem(profile, GetType(), file_path);

  ASSERT_EQ(model->items().size(), 1u);

  // Attempts to add already represented items should be ignored.
  EXPECT_EQ(model->items()[0].get(), item);
  EXPECT_EQ(id, id2);
  EXPECT_TRUE(service->ContainsItem(id));
  EXPECT_TRUE(service->ContainsItem(id2));

  // Remove the holding space item.
  service->RemoveItem(id);
  EXPECT_TRUE(model->items().empty());
  EXPECT_FALSE(service->ContainsItem(id));
  EXPECT_FALSE(service->ContainsItem(id2));

  // Verify `expected_histograms` after "waiting" for metrics debounce.
  // NOTE: Histograms are cumulative so we need to merge `expected_histograms`
  // from the previous state with those of the current.
  task_environment()->FastForwardBy(base::Seconds(30));
  expected_histograms = MergeHistogramSamples(
      expected_histograms, GetExpectedTotalCountV2HistogramSamples(model));
  for (const auto& [name, expected_buckets] : expected_histograms) {
    EXPECT_THAT(histogram_tester.GetAllSamples(name),
                BucketsAreArray(expected_buckets));
  }
}

TEST_P(HoldingSpaceKeyedServiceAddAndRemoveItemTest, AddAndRemoveItemOfType) {
  // Wait for the holding space model to attach.
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space `model` is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());

  // Create a test mount point.
  std::unique_ptr<ScopedTestMountPoint> mount_point =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(mount_point->IsValid());

  // Create a file on the file system.
  const base::FilePath file_path = mount_point->CreateFile(
      /*relative_path=*/base::FilePath("foo"), /*content=*/"foo");

  // Add a holding space item of the type under test.
  const auto& id = GetService(profile)->AddItemOfType(GetType(), file_path);

  // Verify a holding space item has been added to the model.
  ASSERT_EQ(model->items().size(), 1u);

  // Verify holding space `item` metadata.
  HoldingSpaceItem* const item = model->items()[0].get();
  EXPECT_EQ(item->id(), id);
  EXPECT_EQ(item->type(), GetType());
  EXPECT_EQ(item->GetText(), file_path.BaseName().LossyDisplayName());
  EXPECT_EQ(item->file().file_path, file_path);
  EXPECT_EQ(item->file().file_system_url,
            holding_space_util::ResolveFileSystemUrl(profile, file_path));

  // Verify holding space `item` image.
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           GetService(profile)->thumbnail_loader_for_testing(), GetType(),
           file_path)
           ->GetImageSkia()
           .bitmap(),
      *item->image().GetImageSkia().bitmap()));

  // Attempt to add a holding space item of the same type and `file_path`.
  EXPECT_TRUE(GetService(profile)->AddItemOfType(GetType(), file_path).empty());

  // Attempts to add already represented items should be ignored.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0].get(), item);

  // Remove the holding space item.
  GetService(profile)->RemoveItem(id);
  EXPECT_TRUE(model->items().empty());
}

class HoldingSpaceKeyedServiceNearbySharingTest
    : public HoldingSpaceKeyedServiceTest {
 public:
  HoldingSpaceKeyedServiceNearbySharingTest() {
    scoped_feature_list_.InitAndEnableFeature(::features::kNearbySharing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HoldingSpaceKeyedServiceNearbySharingTest, AddNearbyShareItem) {
  // Create a test downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  // Wait for the holding space model.
  HoldingSpaceModelAttachedWaiter(GetProfile()).Wait();

  // Verify that the holding space model gets set even if the holding space
  // keyed service is not explicitly created.
  HoldingSpaceModel* const initial_model =
      HoldingSpaceController::Get()->model();
  EXPECT_TRUE(initial_model);

  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  const base::FilePath item_1_virtual_path("File 1.png");
  // Create a fake nearby shared file on the local file system - later parts of
  // the test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_1_full_path =
      downloads_mount->CreateFile(item_1_virtual_path, "red");
  ASSERT_FALSE(item_1_full_path.empty());

  holding_space_service->AddItemOfType(HoldingSpaceItem::Type::kNearbyShare,
                                       item_1_full_path);

  const base::FilePath item_2_virtual_path = base::FilePath("Alt/File 2.png");
  // Create a fake nearby shared file on the local file system - later parts of
  // the test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_2_full_path =
      downloads_mount->CreateFile(item_2_virtual_path, "blue");
  ASSERT_FALSE(item_2_full_path.empty());
  holding_space_service->AddItemOfType(HoldingSpaceItem::Type::kNearbyShare,
                                       item_2_full_path);

  EXPECT_EQ(initial_model, HoldingSpaceController::Get()->model());
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            holding_space_service->model_for_testing());

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(2u, model->items().size());

  const HoldingSpaceItem* item_1 = model->items()[0].get();
  EXPECT_EQ(item_1_full_path, item_1->file().file_path);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           holding_space_service->thumbnail_loader_for_testing(),
           HoldingSpaceItem::Type::kNearbyShare, item_1_full_path)
           ->GetImageSkia()
           .bitmap(),
      *item_1->image().GetImageSkia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(item_1_virtual_path,
            GetVirtualPathFromUrl(item_1->file().file_system_url,
                                  downloads_mount->name()));
  EXPECT_EQ(u"File 1.png", item_1->GetText());

  const HoldingSpaceItem* item_2 = model->items()[1].get();
  EXPECT_EQ(item_2_full_path, item_2->file().file_path);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           holding_space_service->thumbnail_loader_for_testing(),
           HoldingSpaceItem::Type::kNearbyShare, item_2_full_path)
           ->GetImageSkia()
           .bitmap(),
      *item_2->image().GetImageSkia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(item_2_virtual_path,
            GetVirtualPathFromUrl(item_2->file().file_system_url,
                                  downloads_mount->name()));
  EXPECT_EQ(u"File 2.png", item_2->GetText());
}

// Base class for tests of Photoshop Web integration. Parameterized by the
// binding context to use for the file picker during testing.
class HoldingSpaceKeyedServicePhotoshopWebIntegrationTest
    : public HoldingSpaceKeyedServiceTest,
      public ::testing::WithParamInterface<
          /*file_picker_binding_context=*/GURL> {
 public:
  // The binding context to use for the file picker given test parameterization.
  const GURL& GetFilePickerBindingContext() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceKeyedServicePhotoshopWebIntegrationTest,
    /*file_picker_binding_context=*/
    ::testing::Values(GURL(),
                      GURL("https://google.com/"),
                      GURL("https://photoshop.adobe.com/")));

// Verifies that a Photoshop Web item will be added to the user's Holding Space
// under expected circumstances.
TEST_P(HoldingSpaceKeyedServicePhotoshopWebIntegrationTest,
       AddPhotoshopWebItem) {
  // Cache `profile`.
  TestingProfile* const profile = GetProfile();

  // Wait for `model` attachment and verify initial state.
  HoldingSpaceModelAttachedWaiter(profile).Wait();
  const HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  ASSERT_EQ(model->items().size(), 0u);

  // Create `mount_point`.
  std::unique_ptr<ScopedTestMountPoint> mount_point =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(mount_point->IsValid());

  // Create file and resolve metadata.
  const base::FilePath file_path =
      mount_point->CreateFile(/*relative_path=*/base::FilePath("foo"));
  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile, file_path);
  const HoldingSpaceFile::FileSystemType file_system_type =
      holding_space_util::ResolveFileSystemType(profile, file_system_url);

  // Verify initial histogram state.
  base::HistogramTester histogram_tester;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "HoldingSpace.FileCreatedFromShowSaveFilePicker."),
              IsEmpty());

  // Propagate file creation event from a file picker with the binding context
  // specified by test parameterization.
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->OnFileCreatedFromShowSaveFilePicker(
          GetFilePickerBindingContext(),
          file_manager::util::GetFileManagerFileSystemContext(profile)
              ->CrackURLInFirstPartyContext(file_system_url));

  // A Photoshop Web item should be added to the user's Holding Space iff the
  // binding context for the file picker is from the domain associated with
  // Photoshop Web.
  const bool is_file_picker_binding_context_photoshop_web =
      GetFilePickerBindingContext().DomainIs("photoshop.adobe.com");

  // Verify model state.
  EXPECT_THAT(
      model->items(),
      Conditional(
          is_file_picker_binding_context_photoshop_web,
          ElementsAre(Pointee(AllOf(
              Property(&HoldingSpaceItem::type,
                       HoldingSpaceItem::Type::kPhotoshopWeb),
              Property(&HoldingSpaceItem::file,
                       AllOf(Field(&HoldingSpaceFile::file_path, file_path),
                             Field(&HoldingSpaceFile::file_system_type,
                                   file_system_type),
                             Field(&HoldingSpaceFile::file_system_url,
                                   file_system_url)))))),
          IsEmpty()));

  // Verify histogram state.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HoldingSpace.FileCreatedFromShowSaveFilePicker.Extension"),
              BucketsAre(Bucket(
                  holding_space_metrics::FilePathToExtension(file_path), 1u)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "HoldingSpace.FileCreatedFromShowSaveFilePicker."
          "FilePickerBindingContext"),
      Conditional(
          is_file_picker_binding_context_photoshop_web,
          BucketsAre(Bucket(FilePickerBindingContext::kPhotoshopWeb, 1u)),
          BucketsAre(Bucket(FilePickerBindingContext::kUnknown, 1u))));
}

// Base class for tests of print-to-PDF integration. Parameterized by whether
// tests should use an incognito browser.
class HoldingSpaceKeyedServicePrintToPdfIntegrationTest
    : public HoldingSpaceKeyedServiceTest,
      public testing::WithParamInterface<bool /* from_incognito_profile */> {
 public:
  // Starts a job to print an empty PDF to the specified `file_path`.
  // NOTE: This method will not return until the print job completes.
  void StartPrintToPdfAndWaitForSave(const std::u16string& job_title,
                                     const base::FilePath& file_path) {
    base::RunLoop run_loop;
    pdf_printer_handler_->SetPdfSavedClosureForTesting(run_loop.QuitClosure());
    pdf_printer_handler_->SetPrintToPdfPathForTesting(file_path);

    pdf_printer_handler_->StartPrint(
        job_title,
        /*settings=*/base::Value::Dict(),
        base::MakeRefCounted<base::RefCountedString>(std::string()),
        /*callback=*/base::DoNothing());

    run_loop.Run();
  }

  // Returns true if the test should use an incognito browser, false otherwise.
  bool UseIncognitoBrowser() const { return GetParam(); }

 private:
  // HoldingSpaceKeyedServiceTest:
  void SetUp() override {
    HoldingSpaceKeyedServiceTest::SetUp();

    // Create the PDF printer handler.
    Browser* browser = GetBrowserForPdfPrinterHandler();
    pdf_printer_handler_ = std::make_unique<printing::PdfPrinterHandler>(
        browser->profile(), browser->tab_strip_model()->GetActiveWebContents(),
        /*sticky_settings=*/nullptr);
  }

  void TearDown() override {
    incognito_browser_.reset();
    HoldingSpaceKeyedServiceTest::TearDown();
  }

  Browser* GetBrowserForPdfPrinterHandler() {
    if (!UseIncognitoBrowser()) {
      return browser();
    }
    if (!incognito_browser_) {
      incognito_browser_ =
          CreateBrowserWithTestWindowForParams(Browser::CreateParams(
              profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
              /*user_gesture=*/true));
    }
    return incognito_browser_.get();
  }

  std::unique_ptr<printing::PdfPrinterHandler> pdf_printer_handler_;
  std::unique_ptr<Browser> incognito_browser_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceKeyedServicePrintToPdfIntegrationTest,
                         /*from_incognito_profile=*/::testing::Bool());

// Verifies that print-to-PDF adds an associated item to holding space.
TEST_P(HoldingSpaceKeyedServicePrintToPdfIntegrationTest, AddPrintedPdfItem) {
  // Create a file system mount point.
  std::unique_ptr<ScopedTestMountPoint> mount_point =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(mount_point->IsValid());

  // Cache a pointer to the holding space model.
  const HoldingSpaceModel* model =
      HoldingSpaceKeyedServiceFactory::GetInstance()
          ->GetService(GetProfile())
          ->model_for_testing();

  // Verify that the holding space is initially empty.
  EXPECT_EQ(model->items().size(), 0u);

  // Start a job to print an empty PDF to `file_path`.
  base::FilePath file_path = mount_point->GetRootPath().Append("foo.pdf");
  StartPrintToPdfAndWaitForSave(u"job_title", file_path);

  // Verify that holding space is populated with the expected item.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kPrintedPdf);
  EXPECT_EQ(model->items()[0]->file().file_path, file_path);
}

// Base class for tests of incognito profile integration.
class HoldingSpaceKeyedServiceIncognitoDownloadsTest
    : public HoldingSpaceKeyedServiceTest {
 public:
  // HoldingSpaceKeyedServiceTest:
  TestingProfile* CreateProfile(const std::string& profile_name) override {
    TestingProfile* profile =
        HoldingSpaceKeyedServiceTest::CreateProfile(profile_name);

    // Construct an incognito profile from the primary profile.
    TestingProfile::Builder incognito_profile_builder;
    incognito_profile_builder.SetProfileName(profile->GetProfileUserName());
    incognito_profile_ = incognito_profile_builder.BuildIncognito(profile);
    EXPECT_TRUE(incognito_profile_);
    EXPECT_TRUE(incognito_profile_->IsIncognitoProfile());
    SetUpDownloadManager(incognito_profile_);
    EXPECT_NE(incognito_profile_->GetDownloadManager(),
              profile->GetDownloadManager());

    return profile;
  }

  // Returns the incognito profile spawned from the test's main profile.
  TestingProfile* incognito_profile() { return incognito_profile_; }

 private:
  raw_ptr<TestingProfile, DanglingUntriaged> incognito_profile_ = nullptr;
};

TEST_F(HoldingSpaceKeyedServiceIncognitoDownloadsTest, AddDownloadItem) {
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Create a test downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache current state, file path, received bytes, and total bytes.
  auto current_state = download::DownloadItem::IN_PROGRESS;
  base::FilePath current_path;
  int64_t current_received_bytes = 0;
  int64_t current_total_bytes = 100;

  // Create a fake in-progress download item for the incognito profile and cache
  // a function to update it.
  std::unique_ptr<content::FakeDownloadItem> fake_download_item =
      CreateFakeDownloadItem(incognito_profile(), current_state, current_path,
                             /*target_file_path=*/base::FilePath(),
                             current_received_bytes, current_total_bytes);
  auto UpdateFakeDownloadItem = [&]() {
    fake_download_item->SetDummyFilePath(current_path);
    fake_download_item->SetReceivedBytes(current_received_bytes);
    fake_download_item->SetState(current_state);
    fake_download_item->SetTotalBytes(current_total_bytes);
    fake_download_item->NotifyDownloadUpdated();
  };

  // Verify holding space is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());

  // Update the file path for the download.
  current_path = downloads_mount->CreateFile(base::FilePath("tmp/temp_path"));
  UpdateFakeDownloadItem();

  // Verify that a holding space item is created.
  ASSERT_EQ(1u, model->items().size());
  HoldingSpaceItem* download_item = model->items()[0].get();
  EXPECT_EQ(download_item->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(download_item->file().file_path, current_path);
  EXPECT_EQ(download_item->progress().GetValue(), 0.f);

  // Complete the download.
  current_state = download::DownloadItem::COMPLETE;
  current_path = downloads_mount->CreateFile(base::FilePath("tmp/final_path"));
  current_received_bytes = current_total_bytes;
  UpdateFakeDownloadItem();

  // Verify that a completed holding space item exists.
  ASSERT_EQ(1u, model->items().size());
  download_item = model->items()[0].get();
  EXPECT_EQ(download_item->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(download_item->file().file_path, current_path);
  EXPECT_TRUE(download_item->progress().IsComplete());
}

TEST_F(HoldingSpaceKeyedServiceIncognitoDownloadsTest,
       AddInProgressDownloadItem) {
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Verify the holding space model is empty.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->items().size(), 0u);

  // Create a test downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Cache current state, file paths, received bytes, and total bytes.
  auto current_state = download::DownloadItem::IN_PROGRESS;
  base::FilePath current_path;
  base::FilePath current_target_path;
  int64_t current_received_bytes = 0;
  int64_t current_total_bytes = 100;
  bool current_is_dangerous = false;

  // Create a fake download item and cache a function to update it.
  std::unique_ptr<content::FakeDownloadItem> fake_download_item =
      CreateFakeDownloadItem(incognito_profile(), current_state, current_path,
                             current_target_path, current_received_bytes,
                             current_total_bytes);
  auto UpdateFakeDownloadItem = [&]() {
    fake_download_item->SetDummyFilePath(current_path);
    fake_download_item->SetReceivedBytes(current_received_bytes);
    fake_download_item->SetState(current_state);
    fake_download_item->SetTargetFilePath(current_target_path);
    fake_download_item->SetTotalBytes(current_total_bytes);
    fake_download_item->SetIsDangerous(current_is_dangerous);
    fake_download_item->NotifyDownloadUpdated();
  };

  // Verify that no holding space item has been created since the download does
  // not yet have file path set.
  EXPECT_EQ(model->items().size(), 0u);

  // Update the file paths for the download.
  current_path = downloads_mount->CreateFile(base::FilePath("foo.crdownload"));
  current_target_path = downloads_mount->CreateFile(base::FilePath("foo.png"));
  UpdateFakeDownloadItem();

  // Verify that a holding space item is created.
  ASSERT_EQ(1u, model->items().size());
  HoldingSpaceItem* download_item = model->items()[0].get();
  EXPECT_EQ(download_item->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(download_item->file().file_path, current_path);
  EXPECT_FALSE(download_item->progress().IsComplete());

  // Verify that destroying a profile with an in-progress download destroys
  // the holding space item.
  profile->DestroyOffTheRecordProfile(incognito_profile());
  ASSERT_EQ(0u, model->items().size());
}

class HoldingSpaceSuggestionsDelegateTest
    : public HoldingSpaceKeyedServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  HoldingSpaceSuggestionsDelegateTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kHoldingSpaceSuggestions, GetParam());
  }

  void SetUp() override {
    HoldingSpaceKeyedServiceTest::SetUp();

    // Create mount points to host test files.
    TestingProfile* profile = GetProfile();
    drive_mount_point_ = std::make_unique<ScopedTestMountPoint>(
        "drive_test_mount", storage::kFileSystemTypeDriveFs,
        file_manager::VOLUME_TYPE_TESTING);
    drive_mount_point_->Mount(profile);
    local_mount_point_ = std::make_unique<ScopedTestMountPoint>(
        "local_test_mount", storage::kFileSystemTypeLocal,
        file_manager::VOLUME_TYPE_TESTING);
    local_mount_point_->Mount(profile);

    HoldingSpaceModelAttachedWaiter(profile).Wait();
  }

  void TearDown() override {
    drive_mount_point_.reset();
    local_mount_point_.reset();
    HoldingSpaceKeyedServiceTest::TearDown();
  }

  MockFileSuggestKeyedService* GetFileSuggestKeyedService() {
    return static_cast<MockFileSuggestKeyedService*>(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            GetProfile()));
  }

  ScopedTestMountPoint* drive_mount_point() { return drive_mount_point_.get(); }
  ScopedTestMountPoint* local_mount_point() { return local_mount_point_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ScopedTestMountPoint> drive_mount_point_;
  std::unique_ptr<ScopedTestMountPoint> local_mount_point_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceSuggestionsDelegateTest,
                         /*enable_suggestion_feature=*/testing::Bool());

// Verifies that suggestion refresh through the holding space client is WAI.
TEST_P(HoldingSpaceSuggestionsDelegateTest, SuggestionRefresh) {
  using Type = HoldingSpaceItem::Type;

  // Populate drive and local file suggestions.
  const base::FilePath file_path_1 = drive_mount_point()->CreateArbitraryFile();
  const base::FilePath file_path_2 = local_mount_point()->CreateArbitraryFile();
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kLocalFile, file_path_2,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  // Verify initial suggestions.  Note that suggestions are reversed in the
  // holding space model to account for the fact that items are presented in
  // reverse-chronological order.
  const bool suggestion_feature_enabled =
      features::IsHoldingSpaceSuggestionsEnabled();
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  EXPECT_THAT(GetSuggestionsInModel(*model),
              ::testing::Conditional(
                  suggestion_feature_enabled,
                  ::testing::ElementsAre(
                      std::make_pair(Type::kLocalSuggestion, file_path_2),
                      std::make_pair(Type::kDriveSuggestion, file_path_1)),
                  ::testing::IsEmpty()));

  // Create additional files to back refreshed suggestions.
  const base::FilePath file_path_3 = drive_mount_point()->CreateArbitraryFile();
  const base::FilePath file_path_4 = local_mount_point()->CreateArbitraryFile();

  // Refresh suggestions through the holding space client. Verify that
  // `FileSuggestKeyedService::GetSuggestFileData()` is called if and only if
  // the suggestions feature is enabled.
  EXPECT_CALL(*GetFileSuggestKeyedService(),
              GetSuggestFileData(FileSuggestionType::kDriveFile, ::testing::_))
      .Times(suggestion_feature_enabled ? 1u : 0u)
      .WillOnce(base::test::RunOnceCallback<1u>(
          std::make_optional(std::vector<FileSuggestData>{
              {FileSuggestionType::kDriveFile, file_path_3,
               /*title=*/std::nullopt,
               /*new_prediction_reason=*/std::nullopt,
               /*modified_time=*/std::nullopt,
               /*viewed_time=*/std::nullopt,
               /*shared_time=*/std::nullopt,
               /*new_score=*/std::nullopt,
               /*drive_file_id=*/std::nullopt,
               /*icon_url=*/std::nullopt}})));
  EXPECT_CALL(*GetFileSuggestKeyedService(),
              GetSuggestFileData(FileSuggestionType::kLocalFile, ::testing::_))
      .Times(suggestion_feature_enabled ? 1u : 0u)
      .WillOnce(base::test::RunOnceCallback<1u>(
          std::make_optional(std::vector<FileSuggestData>{
              {FileSuggestionType::kLocalFile, file_path_4,
               /*title=*/std::nullopt,
               /*new_prediction_reason=*/std::nullopt,
               /*modified_time=*/std::nullopt,
               /*viewed_time=*/std::nullopt,
               /*shared_time=*/std::nullopt,
               /*new_score=*/std::nullopt,
               /*drive_file_id=*/std::nullopt,
               /*icon_url=*/std::nullopt}})));
  HoldingSpaceController::Get()->client()->RefreshSuggestions();

  // Verify that all suggestions have been updated in the model if and only if
  // the suggestions feature is enabled.
  EXPECT_THAT(GetSuggestionsInModel(*model),
              ::testing::Conditional(
                  suggestion_feature_enabled,
                  ::testing::ElementsAre(
                      std::make_pair(Type::kLocalSuggestion, file_path_4),
                      std::make_pair(Type::kDriveSuggestion, file_path_3)),
                  ::testing::IsEmpty()));
}

// Verifies that suggestion removal through the holding space client is WAI.
TEST_P(HoldingSpaceSuggestionsDelegateTest, SuggestionRemoval) {
  using Type = HoldingSpaceItem::Type;

  // Populate drive and local file suggestions.
  const base::FilePath file_path_1 = drive_mount_point()->CreateArbitraryFile();
  const base::FilePath file_path_2 = local_mount_point()->CreateArbitraryFile();
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kLocalFile, file_path_2,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  // Verify initial suggestions.  Note that suggestions are reversed in the
  // holding space model to account for the fact that items are presented in
  // reverse-chronological order.
  const bool suggestion_feature_enabled =
      features::IsHoldingSpaceSuggestionsEnabled();
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  EXPECT_THAT(GetSuggestionsInModel(*model),
              ::testing::Conditional(
                  suggestion_feature_enabled,
                  ::testing::ElementsAre(
                      std::make_pair(Type::kLocalSuggestion, file_path_2),
                      std::make_pair(Type::kDriveSuggestion, file_path_1)),
                  ::testing::IsEmpty()));

  // Remove all suggestions through the holding space client. Verify that
  // `FileSuggestKeyedService::RemoveSuggestionsAndNotify()` is called if and
  // only if the suggestions feature is enabled.
  EXPECT_CALL(*GetFileSuggestKeyedService(),
              RemoveSuggestionsAndNotify(
                  std::vector<base::FilePath>({file_path_1, file_path_2})))
      .Times(suggestion_feature_enabled ? 1u : 0u);
  HoldingSpaceController::Get()->client()->RemoveSuggestions(
      {file_path_1, file_path_2});
  task_environment()->FastForwardBy(base::Seconds(1));

  // Verify that all suggestions have been removed from the `model`.
  EXPECT_THAT(GetSuggestionsInModel(*model), IsEmpty());
}

TEST_P(HoldingSpaceSuggestionsDelegateTest, VerifySuggestionsInModel) {
  const base::FilePath file_path_1 = drive_mount_point()->CreateArbitraryFile();

  // Update Drive file suggestions. Fast-forward to ensure the suggestion fetch
  // completes.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  const bool suggestion_feature_enabled =
      features::IsHoldingSpaceSuggestionsEnabled();

  // Populate the expected suggestions array if the holding space suggestion
  // feature is enabled. There should be no suggestions in the model when the
  // feature is disabled.
  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>> expected;
  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }

  // Check the model after Drive file suggestions update.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  const base::FilePath file_path_2 = local_mount_point()->CreateArbitraryFile();

  // Update local file suggestions and check the model.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kLocalFile, file_path_2,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->RunUntilIdle();

  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path_2},
                {HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  const base::FilePath file_path_3 = drive_mount_point()->CreateArbitraryFile();

  // Update Drive file suggestions again and check the model.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/
      std::vector<FileSuggestData>{{FileSuggestionType::kDriveFile, file_path_1,
                                    /*title=*/std::nullopt,
                                    /*new_prediction_reason=*/std::nullopt,
                                    /*modified_time=*/std::nullopt,
                                    /*viewed_time=*/std::nullopt,
                                    /*shared_time=*/std::nullopt,
                                    /*new_score=*/std::nullopt,
                                    /*drive_file_id=*/std::nullopt,
                                    /*icon_url=*/std::nullopt},
                                   {FileSuggestionType::kDriveFile, file_path_3,
                                    /*title=*/std::nullopt,
                                    /*new_prediction_reason=*/std::nullopt,
                                    /*modified_time=*/std::nullopt,
                                    /*viewed_time=*/std::nullopt,
                                    /*shared_time=*/std::nullopt,
                                    /*new_score=*/std::nullopt,
                                    /*drive_file_id=*/std::nullopt,
                                    /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path_2},
                {HoldingSpaceItem::Type::kDriveSuggestion, file_path_3},
                {HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Update Drive file suggestions with an empty array.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{});
  task_environment()->FastForwardBy(base::Seconds(1));

  // Drive file suggestions should be removed from the model if suggestions are
  // enabled.
  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path_2}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Update local file suggestions with an empty array.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{});
  task_environment()->FastForwardBy(base::Seconds(1));

  // There should be no suggestions in the model.
  expected.clear();
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);
}

TEST_P(HoldingSpaceSuggestionsDelegateTest, DownloadsFolderNotSuggested) {
  auto downloads_mount =
      local_mount_point()->CreateAndMountDownloads(GetProfile());
  auto downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(GetProfile());
  auto other_folder_path = downloads_path.Append("contained_folder");
  ASSERT_TRUE(base::CreateDirectory(other_folder_path));
  const base::FilePath file_path = local_mount_point()->CreateArbitraryFile();

  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kLocalFile, downloads_path,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt},
          {FileSuggestionType::kLocalFile, other_folder_path,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt},
          {FileSuggestionType::kLocalFile, file_path,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>> expected;
  if (features::IsHoldingSpaceSuggestionsEnabled()) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path},
                {HoldingSpaceItem::Type::kLocalSuggestion, other_folder_path}};
  }

  EXPECT_EQ(GetSuggestionsInModel(*HoldingSpaceController::Get()->model()),
            expected);
}

TEST_P(HoldingSpaceSuggestionsDelegateTest, PinAndUnpinSuggestions) {
  const base::FilePath file_path_1 = drive_mount_point()->CreateArbitraryFile();
  const GURL file_system_url_1 = GetFileSystemUrl(GetProfile(), file_path_1);
  const HoldingSpaceFile::FileSystemType file_system_type_1 =
      holding_space_util::ResolveFileSystemType(GetProfile(),
                                                file_system_url_1);

  // Update Drive file suggestions. Fast-forward to ensure the suggestion fetch
  // completes.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  const bool suggestion_feature_enabled =
      features::IsHoldingSpaceSuggestionsEnabled();

  // Populate the expected suggestions array if the holding space suggestion
  // feature is enabled. There should be no suggestions in the model when the
  // feature is disabled.
  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>> expected;
  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }

  // Check the model after Drive file suggestions update.
  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  const base::FilePath file_path_2 = local_mount_point()->CreateArbitraryFile();

  // Update local file suggestions and check the model.
  GetFileSuggestKeyedService()->SetSuggestionsForType(
      FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kLocalFile, file_path_2,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  task_environment()->RunUntilIdle();

  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path_2},
                {HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Pin the suggested Drive file and verify that the suggestion is removed
  // from the model if suggestions are enabled.
  auto pinned_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kPinnedFile,
      HoldingSpaceFile(file_path_1, file_system_type_1, file_system_url_1),
      base::BindOnce(&CreateTestHoldingSpaceImage));
  const auto& pinned_item_id = pinned_item->id();
  model->AddItem(std::move(pinned_item));
  task_environment()->RunUntilIdle();

  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path_2}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Unpin the suggested Drive file and verify that the suggestion is re-added
  // to the model if suggestions are enabled.
  model->RemoveItem(pinned_item_id);
  task_environment()->RunUntilIdle();

  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kLocalSuggestion, file_path_2},
                {HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Add an uninitialized pinned item for the suggested local file to the model
  // and verify that there is no change to the model's suggestions.
  auto* uninitialized_pinned_item_ptr = AddUninitializedItem(
      model, HoldingSpaceItem::Type::kPinnedFile, file_path_2);

  // The `expected` suggestions should not have changed.
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Remove the suggested local file's uninitialized pinned item and verify
  // that there is no change to the model's suggestions.
  model->RemoveItem(uninitialized_pinned_item_ptr->id());

  // The `expected` suggestions should not have changed.
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Add an uninitialized pinned item for the suggested local file to the model
  // and verify that there is no change to the model's suggestions.
  auto* partially_initialized_pinned_item_ptr = AddUninitializedItem(
      model, HoldingSpaceItem::Type::kPinnedFile, file_path_2);

  // The `expected` suggestions should not have changed.
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);

  // Initialize the pinned item for the suggested local file and verify that
  // the suggestion is removed from the model if suggestions are enabled.
  model->InitializeOrRemoveItem(
      partially_initialized_pinned_item_ptr->id(),
      HoldingSpaceFile(file_path_2, HoldingSpaceFile::FileSystemType::kTest,
                       GetFileSystemUrl(GetProfile(), file_path_2)));
  task_environment()->RunUntilIdle();

  if (suggestion_feature_enabled) {
    expected = {{HoldingSpaceItem::Type::kDriveSuggestion, file_path_1}};
  }
  EXPECT_EQ(GetSuggestionsInModel(*model), expected);
}

// Verifies the file suggestion update on a profile with restored suggestions.
TEST_P(HoldingSpaceSuggestionsDelegateTest, RestoreSuggestions) {
  const base::FilePath drive_file = drive_mount_point()->CreateArbitraryFile();
  const GURL drive_file_system_url = GetFileSystemUrl(GetProfile(), drive_file);
  const HoldingSpaceFile::FileSystemType drive_file_system_type =
      holding_space_util::ResolveFileSystemType(GetProfile(),
                                                drive_file_system_url);

  std::unique_ptr<HoldingSpaceItem> drive_file_suggestion =
      HoldingSpaceItem::CreateFileBackedItem(
          HoldingSpaceItem::Type::kDriveSuggestion,
          HoldingSpaceFile(drive_file, drive_file_system_type,
                           drive_file_system_url),
          base::BindOnce(&CreateTestHoldingSpaceImage));

  // Create a secondary profile with a persisted drive file suggestion.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_items;
        persisted_items.Append(drive_file_suggestion->Serialize());
        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(std::move(persisted_items)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  // Activate `secondary_profile`. Wait until the model updates.
  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ItemsInitializedWaiter(secondary_holding_space_model).Wait();
  const bool suggestion_feature_enabled =
      features::IsHoldingSpaceSuggestionsEnabled();
  EXPECT_EQ(secondary_holding_space_model->items().size(),
            suggestion_feature_enabled ? 1u : 0u);

  // Update local file suggestions on the secondary profile. Fast-forward to
  // ensure the suggestion fetch completes.
  const base::FilePath local_file = local_mount_point()->CreateArbitraryFile();
  static_cast<MockFileSuggestKeyedService*>(
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile))
      ->SetSuggestionsForType(FileSuggestionType::kLocalFile,
                              /*suggestions=*/std::vector<FileSuggestData>{
                                  {FileSuggestionType::kLocalFile, local_file,
                                   /*title=*/std::nullopt,
                                   /*new_prediction_reason=*/std::nullopt,
                                   /*modified_time=*/std::nullopt,
                                   /*viewed_time=*/std::nullopt,
                                   /*shared_time=*/std::nullopt,
                                   /*new_score=*/std::nullopt,
                                   /*drive_file_id=*/std::nullopt,
                                   /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  const auto& model_items = secondary_holding_space_model->items();
  if (suggestion_feature_enabled) {
    // The drive and local file suggestions should coexist in the model.
    ASSERT_EQ(model_items.size(), 2u);
    EXPECT_EQ(model_items[0]->file().file_path, local_file);
    EXPECT_EQ(model_items[1]->file().file_path, drive_file);
  } else {
    EXPECT_TRUE(model_items.empty());
  }
}

// Verifies by updating file suggestions in the holding space model which
// contains the suggested files from an unmounted file system.
TEST_P(HoldingSpaceSuggestionsDelegateTest, UpdateSuggestionsWithDelayedMount) {
  auto delayed_mount = std::make_unique<ScopedTestMountPoint>(
      "drivefs-delayed_mount",
      /*file_system_type=*/storage::kFileSystemTypeDriveFs,
      /*volume_type=*/file_manager::VOLUME_TYPE_GOOGLE_DRIVE);
  const base::FilePath delayed_mount_file_path =
      delayed_mount->GetRootPath().Append("delayed file");
  auto delayed_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDriveSuggestion,
      HoldingSpaceFile(delayed_mount_file_path,
                       HoldingSpaceFile::FileSystemType::kTest,
                       GURL("filesystem:fake")),
      base::BindOnce(&CreateTestHoldingSpaceImage));

  // Create a secondary profile with a persisted delayed file suggestion.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value::List persisted_items;
        persisted_items.Append(delayed_holding_space_item->Serialize());
        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value(std::move(persisted_items)),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  // Activate `secondary_profile`. Wait until the model updates.
  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();
  const bool suggestion_feature_enabled =
      features::IsHoldingSpaceSuggestionsEnabled();
  EXPECT_EQ(secondary_holding_space_model->items().size(),
            suggestion_feature_enabled ? 1u : 0u);

  // Update with a local file suggestion.
  const base::FilePath local_file = local_mount_point()->CreateArbitraryFile();
  static_cast<MockFileSuggestKeyedService*>(
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile))
      ->SetSuggestionsForType(FileSuggestionType::kLocalFile,
                              /*suggestions=*/std::vector<FileSuggestData>{
                                  {FileSuggestionType::kLocalFile, local_file,
                                   /*title=*/std::nullopt,
                                   /*new_prediction_reason=*/std::nullopt,
                                   /*modified_time=*/std::nullopt,
                                   /*viewed_time=*/std::nullopt,
                                   /*shared_time=*/std::nullopt,
                                   /*new_score=*/std::nullopt,
                                   /*drive_file_id=*/std::nullopt,
                                   /*icon_url=*/std::nullopt}});
  task_environment()->FastForwardBy(base::Seconds(1));

  const auto& model_items = secondary_holding_space_model->items();
  if (suggestion_feature_enabled) {
    ASSERT_EQ(model_items.size(), 2u);
    EXPECT_EQ(model_items[0]->file().file_path, local_file);
    EXPECT_EQ(model_items[0]->type(), HoldingSpaceItem::Type::kLocalSuggestion);
    EXPECT_TRUE(model_items[0]->IsInitialized());

    EXPECT_EQ(model_items[1]->file().file_path, delayed_mount_file_path);
    EXPECT_EQ(model_items[1]->type(), HoldingSpaceItem::Type::kDriveSuggestion);
    EXPECT_FALSE(model_items[1]->IsInitialized());
  } else {
    EXPECT_TRUE(model_items.empty());
  }
}

}  // namespace ash
