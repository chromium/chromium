// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/disks/disk_mount_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/image_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "components/account_id/account_id.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/async_file_test_helper.h"
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

using holding_space::ScopedTestMountPoint;

namespace {

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

std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.push_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

std::unique_ptr<KeyedService> BuildArcIntentHelperBridge(
    content::BrowserContext* context) {
  EXPECT_TRUE(arc::ArcServiceManager::Get());
  EXPECT_TRUE(arc::ArcServiceManager::Get()->arc_bridge_service());
  return std::make_unique<arc::ArcIntentHelperBridge>(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */,
      ash::disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

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
    if (IsModelAttached())
      return;

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
  }

 private:
  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override {
    if (wait_loop_ && IsModelAttached())
      wait_loop_->Quit();
  }

  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override {}

  bool IsModelAttached() const {
    HoldingSpaceKeyedService* const holding_space_service =
        HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile_);
    return HoldingSpaceController::Get()->model() ==
           holding_space_service->model_for_testing();
  }

  Profile* const profile_;
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
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override {
    if (!wait_loop_) {
      // `wait_loop_` is nullptr, if wait has not yet been called.
      if (item == wait_item_) {
        wait_item_updated_ = true;
      }
      return;
    }
    if (item == wait_item_)
      wait_loop_->Quit();
  }

  const HoldingSpaceItem* wait_item_ = nullptr;
  std::unique_ptr<base::RunLoop> wait_loop_;
  bool wait_item_updated_ = false;

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
    if (FilteredItemsInitialized())
      return;

    base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
        model_observer{this};
    model_observer.Observe(model_);

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
    filter_ = ItemFilter();
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    if (FilteredItemsInitialized())
      wait_loop_->Quit();
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    if (FilteredItemsInitialized())
      wait_loop_->Quit();
  }

 private:
  bool FilteredItemsInitialized() const {
    for (auto& item : model_->items()) {
      if (filter_ && !filter_.Run(item.get()))
        continue;
      if (!item->IsInitialized())
        return false;
    }
    return true;
  }

  HoldingSpaceModel* const model_;
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
    for (auto& observer : observers_)
      observer.ManagerGoingDown(this);
  }

  void NotifyDownloadCreated(download::DownloadItem* item) {
    for (auto& observer : observers_)
      observer.OnDownloadCreated(this, item);
  }

 private:
  base::ObserverList<content::DownloadManager::Observer>::Unchecked observers_;
};

}  // namespace

class HoldingSpaceKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  HoldingSpaceKeyedServiceTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
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
    // The test's task environment starts with a mock time close to the Unix
    // epoch, but the files that back holding space items are created with
    // accurate timestamps. Advance the clock so that the test's mock time and
    // the time used for file operations are in sync for file age calculations.
    task_environment()->AdvanceClock(base::subtle::TimeNowIgnoringOverride() -
                                     base::Time::Now());
    // Needed by `file_manager::VolumeManager`.
    ash::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager);
    BrowserWithTestWindowTest::SetUp();
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    ash::disks::DiskMountManager::Shutdown();
  }

  TestingProfile* CreateProfile() override {
    const std::string kPrimaryProfileName = "primary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));

    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    GetSessionControllerClient()->AddUserSession(kPrimaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);

    TestingProfile* profile = profile_manager()->CreateTestingProfile(
        kPrimaryProfileName,
        /*testing_factories=*/{
            {arc::ArcIntentHelperBridge::GetFactory(),
             base::BindRepeating(&BuildArcIntentHelperBridge)},
            {file_manager::VolumeManagerFactory::GetInstance(),
             base::BindRepeating(&BuildVolumeManager)}});
    SetUpDownloadManager(profile);
    return profile;
  }

  TestingProfile* CreateSecondaryProfile(
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs = nullptr) {
    const std::string kSecondaryProfileName = "secondary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    TestingProfile* profile = profile_manager()->CreateTestingProfile(
        kSecondaryProfileName, std::move(prefs), u"Test profile",
        1 /*avatar_id*/,
        /*testing_factories=*/
        {{arc::ArcIntentHelperBridge::GetFactory(),
          base::BindRepeating(&BuildArcIntentHelperBridge)},
         {file_manager::VolumeManagerFactory::GetInstance(),
          base::BindRepeating(&BuildVolumeManager)}});
    SetUpDownloadManager(profile);
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
  FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::map<Profile*, testing::NiceMock<MockDownloadManager>*>
      download_managers_;
  arc::ArcServiceManager arc_service_manager_;
};

TEST_F(HoldingSpaceKeyedServiceTest, GuestUserProfile) {
  // Construct a guest session profile.
  TestingProfile::Builder guest_profile_builder;
  guest_profile_builder.SetGuestSession();
  guest_profile_builder.SetProfileName("guest_profile");
  guest_profile_builder.AddTestingFactories(
      {{arc::ArcIntentHelperBridge::GetFactory(),
        base::BindRepeating(&BuildArcIntentHelperBridge)},
       {file_manager::VolumeManagerFactory::GetInstance(),
        base::BindRepeating(&BuildVolumeManager)}});
  std::unique_ptr<TestingProfile> guest_profile = guest_profile_builder.Build();

  // Service instances should be created for guest sessions but note that the
  // service factory will redirect to use the primary OTR profile.
  ASSERT_TRUE(guest_profile);
  ASSERT_FALSE(guest_profile->IsOffTheRecord());
  HoldingSpaceKeyedService* const guest_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          guest_profile.get());
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
          guest_profile.get(), Profile::OTRProfileID::CreateUniqueForTesting());
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

TEST_F(HoldingSpaceKeyedServiceTest, OffTheRecordProfile) {
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

TEST_F(HoldingSpaceKeyedServiceTest, SecondaryUserProfile) {
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

// Verifies that updates to the holding space model are persisted.
TEST_F(HoldingSpaceKeyedServiceTest, UpdatePersistentStorage) {
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

  base::Value persisted_holding_space_items(base::Value::Type::LIST);

  // Verify persistent storage is updated when adding each type of item.
  for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
    const base::FilePath file_path = downloads_mount->CreateArbitraryFile();
    const GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);

    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        type, file_path, file_system_url,
        base::BindOnce(
            &holding_space_util::ResolveImage,
            primary_holding_space_service->thumbnail_loader_for_testing()));

    persisted_holding_space_items.Append(holding_space_item->Serialize());
    primary_holding_space_model->AddItem(std::move(holding_space_item));

    EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }

  // Verify persistent storage is updated when removing each type of item.
  while (!primary_holding_space_model->items().empty()) {
    const auto* holding_space_item =
        primary_holding_space_model->items()[0].get();

    persisted_holding_space_items.EraseListIter(
        persisted_holding_space_items.GetListDeprecated().begin());
    primary_holding_space_model->RemoveItem(holding_space_item->id());

    EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
}

// Verifies that only finalized holding space items are persisted and that,
// once finalized, previously in progress holding space items are persisted at
// the appropriate index.
TEST_F(HoldingSpaceKeyedServiceTest, PersistenceOfInProgressItems) {
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
                ->GetListDeprecated()
                .size(),
            0u);

  // Add a finalized item to holding space. Because the item is finalized, it
  // should immediately be added to persistent storage.
  base::FilePath file_path = downloads_mount->CreateArbitraryFile();
  auto finalized_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, file_path,
      GetFileSystemUrl(GetProfile(), file_path),
      base::BindOnce(&holding_space_util::ResolveImage,
                     holding_space_service->thumbnail_loader_for_testing()));
  auto* finalized_holding_space_item_ptr = finalized_holding_space_item.get();
  holding_space_model->AddItem(std::move(finalized_holding_space_item));

  base::Value persisted_holding_space_items(base::Value::Type::LIST);
  persisted_holding_space_items.Append(
      finalized_holding_space_item_ptr->Serialize());

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Add an in-progress item to holding space. Because the item is in progress,
  // it should *not* be added to persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  auto in_progress_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, file_path,
      GetFileSystemUrl(GetProfile(), file_path),
      HoldingSpaceProgress(/*current_bytes=*/50, /*total_bytes=*/100),
      base::BindOnce(&holding_space_util::ResolveImage,
                     holding_space_service->thumbnail_loader_for_testing()));
  auto* in_progress_holding_space_item_ptr =
      in_progress_holding_space_item.get();
  holding_space_model->AddItem(std::move(in_progress_holding_space_item));

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Add another finalized item to holding space. Because the item is finalized,
  // it should immediately be added to persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  finalized_holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, file_path,
      GetFileSystemUrl(GetProfile(), file_path),
      base::BindOnce(&holding_space_util::ResolveImage,
                     holding_space_service->thumbnail_loader_for_testing()));
  finalized_holding_space_item_ptr = finalized_holding_space_item.get();
  holding_space_model->AddItem(std::move(finalized_holding_space_item));

  persisted_holding_space_items.Append(
      finalized_holding_space_item_ptr->Serialize());

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Update the file path for a finalized item. Because the item is finalized,
  // it should be updated immediately in persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  holding_space_model->UpdateItem(finalized_holding_space_item_ptr->id())
      ->SetBackingFile(file_path, GetFileSystemUrl(GetProfile(), file_path));

  ASSERT_EQ(persisted_holding_space_items.GetListDeprecated().size(), 2u);
  persisted_holding_space_items.GetListDeprecated()[1u] =
      finalized_holding_space_item_ptr->Serialize();

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Update the file path for the in-progress item. Because the item is still in
  // progress, it should not be added/updated to/in persistent storage.
  file_path = downloads_mount->CreateArbitraryFile();
  holding_space_model->UpdateItem(in_progress_holding_space_item_ptr->id())
      ->SetBackingFile(file_path, GetFileSystemUrl(GetProfile(), file_path));

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Update the progress for the in-progress item. Because the item is still in
  // progress it should not be added/updated to/in persistent storage.
  holding_space_model->UpdateItem(in_progress_holding_space_item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/75, /*total_bytes=*/100));

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  // Mark the in-progress item as finalized. Because the item is finalized, it
  // should be added to persistent storage at the appropriate index.
  holding_space_model->UpdateItem(in_progress_holding_space_item_ptr->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100));

  ASSERT_EQ(persisted_holding_space_items.GetListDeprecated().size(), 2u);
  persisted_holding_space_items.Insert(
      persisted_holding_space_items.GetListDeprecated().begin() + 1u,
      in_progress_holding_space_item_ptr->Serialize());

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);
}

// Verifies that when a file backing a holding space item is moved, the holding
// space item is updated in place and persistence storage is updated.
TEST_F(HoldingSpaceKeyedServiceTest, UpdatePersistentStorageAfterMove) {
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

  base::Value persisted_holding_space_items(base::Value::Type::LIST);

  // Verify persistent storage is updated when adding each type of item.
  for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
    // Note that each item is being added to a unique parent directory so that
    // moving the parent directory later will not affect other items.
    const base::FilePath file_path = downloads_mount->CreateFile(
        base::FilePath(base::NumberToString(static_cast<int>(type)))
            .Append("foo.txt"),
        /*content=*/std::string());
    const GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);

    // Create the holding space item.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        type, file_path, file_system_url,
        base::BindOnce(
            &holding_space_util::ResolveImage,
            primary_holding_space_service->thumbnail_loader_for_testing()));

    // Add the holding space item to the model and verify persistence.
    persisted_holding_space_items.Append(holding_space_item->Serialize());
    primary_holding_space_model->AddItem(std::move(holding_space_item));
    EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }

  // Verify persistent storage is updated when moving each type of item and
  // that the holding space items themselves are updated in place.
  for (size_t i = 0; i < primary_holding_space_model->items().size(); ++i) {
    const auto* holding_space_item =
        primary_holding_space_model->items()[i].get();

    // Rename the file backing the holding space item.
    base::FilePath file_path = holding_space_item->file_path();
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
    ASSERT_EQ(holding_space_item->file_path(), new_file_path);
    ASSERT_EQ(holding_space_item->file_system_url(), new_file_path_url);
    ASSERT_EQ(holding_space_item->GetText(),
              new_file_path.BaseName().LossyDisplayName());

    // Verify that persistence has been updated.
    persisted_holding_space_items.GetListDeprecated()[i] =
        holding_space_item->Serialize();
    ASSERT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);

    // Cache the base name of the file backing the holding space item as it will
    // not change due to rename of the holding space item's parent directory.
    base::FilePath base_name = holding_space_item->file_path().BaseName();

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
    ASSERT_EQ(holding_space_item->file_path(), new_file_path);
    ASSERT_EQ(holding_space_item->file_system_url(), new_file_path_url);
    ASSERT_EQ(holding_space_item->GetText(),
              new_file_path.BaseName().LossyDisplayName());

    // Verify that persistence has been updated.
    persisted_holding_space_items.GetListDeprecated()[i] =
        holding_space_item->Serialize();
    ASSERT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
}

// TODO(crbug.com/1170667): Fix flakes and re-enable.
// Tests that holding space item's image representation gets updated when the
// backing file is changed using move operation. Furthermore, verifies that
// conflicts caused by moving a holding space item file to another path present
// in the holding space get resolved.
TEST_F(HoldingSpaceKeyedServiceTest, DISABLED_UpdateItemsOverwrittenByMove) {
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
  };
  struct TestCase {
    ItemInfo src;
    ItemInfo dst;
  };
  std::map<HoldingSpaceItem::Type, TestCase> test_config;

  base::Value persisted_holding_space_items(base::Value::Type::LIST);

  // Configure holding space state for the test. For each item adds two holding
  // space items to the model - "src" and "dst" (during the test, the src item's
  // file will be moved to the dst item's path).
  for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
    auto add_item = [&](const std::string& file_name, ItemInfo* info) {
      info->path = downloads_mount->CreateFile(
          base::FilePath(base::NumberToString(static_cast<int>(type)))
              .Append(file_name),
          /*content=*/std::string());
      info->file_system_url = GetFileSystemUrl(GetProfile(), info->path);

      // Create the holding space item.
      auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
          type, info->path, info->file_system_url,
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

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items);

  base::Value final_persisted_holding_space_items(base::Value::Type::LIST);
  // Runs the test logic.
  for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
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
    ASSERT_EQ(
        storage::AsyncFileTestHelper::Move(
            context,
            context->CrackURLInFirstPartyContext(
                GetFileSystemUrl(GetProfile(), path_not_in_holding_space)),
            context->CrackURLInFirstPartyContext(src_item->file_system_url())),
        base::File::FILE_OK);

    image_update_waiter.Wait();

    ASSERT_EQ(src_item,
              primary_holding_space_model->GetItem(test_case.src.item_id));
    EXPECT_TRUE(primary_holding_space_model->GetItem(test_case.dst.item_id));

    ASSERT_EQ(src_item->file_path(), test_case.src.path);
    ASSERT_EQ(src_item->file_system_url(), test_case.src.file_system_url);

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
    ASSERT_EQ(src_item->file_path(), test_case.dst.path);
    ASSERT_EQ(src_item->file_system_url(), test_case.dst.file_system_url);

    final_persisted_holding_space_items.Append(item->Serialize());
  }

  EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            final_persisted_holding_space_items);
}

// Verifies that the holding space model is restored from persistence. Note that
// when restoring from persistence, existence of backing files is verified and
// any stale holding space items are removed.
TEST_F(HoldingSpaceKeyedServiceTest, RestorePersistentStorage) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value persisted_holding_space_items_after_restoration(
      base::Value::Type::LIST);

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value persisted_holding_space_items_before_restoration(
            base::Value::Type::LIST);

        // Persist some holding space items of each type.
        for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          // We expect the `fresh_holding_space_item` to still be in persistence
          // after model restoration since its backing file exists.
          persisted_holding_space_items_after_restoration.Append(
              fresh_holding_space_item->Serialize());

          // We expect the `fresh_holding_space_item` to be restored from
          // persistence since its backing file exists.
          restored_holding_space_items.push_back(
              std::move(fresh_holding_space_item));

          base::FilePath file_path = downloads_mount->GetRootPath().AppendASCII(
              base::UnguessableToken::Create().ToString());
          auto stale_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file_path, GURL("filesystem:fake_file_system_url"),
                  base::BindOnce(&CreateTestHoldingSpaceImage));

          // NOTE: While the `stale_holding_space_item` is persisted here, we do
          // *not* expect it to be restored or to be persisted after model
          // restoration since its backing file does *not* exist.
          persisted_holding_space_items_before_restoration.Append(
              stale_holding_space_item->Serialize());
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value::ToUniquePtrValue(
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
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);
}

// Verifies that items from volumes that are not immediately mounted during
// startup get restored into the holding space.
TEST_F(HoldingSpaceKeyedServiceTest,
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
  base::Value persisted_holding_space_items_after_restoration(
      base::Value::Type::LIST);
  base::Value persisted_holding_space_items_after_delayed_mount(
      base::Value::Type::LIST);

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value persisted_holding_space_items_before_restoration(
            base::Value::Type::LIST);

        // Persist some holding space items of each type.
        for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
          const base::FilePath delayed_mount_file =
              delayed_mount->GetRootPath().Append(delayed_mount_file_name);
          auto delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, delayed_mount_file, GURL("filesystem:fake"),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          // The item should be restored after delayed volume mount, and remain
          // in persistent storage.
          persisted_holding_space_items_before_restoration.Append(
              delayed_holding_space_item->Serialize());
          persisted_holding_space_items_after_restoration.Append(
              delayed_holding_space_item->Serialize());
          persisted_holding_space_items_after_delayed_mount.Append(
              delayed_holding_space_item->Serialize());
          restored_holding_space_items.push_back(
              std::move(delayed_holding_space_item));

          const base::FilePath non_existent_path =
              delayed_mount->GetRootPath().Append("non-existent");
          auto non_existant_delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, non_existent_path, GURL("filesystem:fake"),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          // The item should be removed from the model and persistent storage
          // after delayed volume mount (when it can be confirmed the backing
          // file does not exist) - the item should remain in persistent storage
          // until the associated volume is mounted.
          persisted_holding_space_items_before_restoration.Append(
              non_existant_delayed_holding_space_item->Serialize());
          persisted_holding_space_items_after_restoration.Append(
              non_existant_delayed_holding_space_item->Serialize());

          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          // The item should be immediately added to the model, and remain in
          // the persistent storage.
          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());
          initialized_items_before_delayed_mount.push_back(
              fresh_holding_space_item->id());
          persisted_holding_space_items_after_restoration.Append(
              fresh_holding_space_item->Serialize());
          persisted_holding_space_items_after_delayed_mount.Append(
              fresh_holding_space_item->Serialize());
          restored_holding_space_items.push_back(
              std::move(fresh_holding_space_item));
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value::ToUniquePtrValue(
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
                    item->file_path());
              }));

  std::vector<std::string> initialized_items;
  for (const auto& item : secondary_holding_space_model->items()) {
    if (item->IsInitialized())
      initialized_items.push_back(item->id());
  }
  EXPECT_EQ(initialized_items_before_delayed_mount, initialized_items);

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
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
    EXPECT_EQ(item->file_path(), item->file_path());
    // NOTE: `restored_item` was created with a fake file system URL (as it
    // could not be properly resolved at the time of item creation).
    EXPECT_EQ(item->file_system_url(),
              GetFileSystemUrl(secondary_profile, restored_item->file_path()));
  }

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_delayed_mount);
}

// Verifies that items from volumes that are not immediately mounted during
// startup get restored into the holding space - same as
// RestorePersistentStorageForDelayedVolumeMount, but the volume gets mounted
// while item restoration is in progress.
TEST_F(HoldingSpaceKeyedServiceTest,
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
  base::Value persisted_holding_space_items_after_delayed_mount(
      base::Value::Type::LIST);

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value persisted_holding_space_items_before_restoration(
            base::Value::Type::LIST);

        // Persist some holding space items of each type.
        for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
          const base::FilePath delayed_mount_file =
              delayed_mount->GetRootPath().Append(delayed_mount_file_name);
          auto delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, delayed_mount_file, GURL("filesystem:fake"),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          // The item should be restored after delayed volume mount, and remain
          // in persistent storage.
          persisted_holding_space_items_before_restoration.Append(
              delayed_holding_space_item->Serialize());
          persisted_holding_space_items_after_delayed_mount.Append(
              delayed_holding_space_item->Serialize());
          restored_holding_space_items.push_back(
              std::move(delayed_holding_space_item));

          base::FilePath non_existent_path =
              delayed_mount->GetRootPath().Append("non-existent");
          auto non_existant_delayed_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, non_existent_path, GURL("filesystem:fake"),
                  base::BindOnce(&CreateTestHoldingSpaceImage));
          // The item should be removed from the model and persistent storage
          // after delayed volume mount (when it can be confirmed the backing
          // file does not exist) - the item should remain in persistent storage
          // until the associated volume is mounted.
          persisted_holding_space_items_before_restoration.Append(
              non_existant_delayed_holding_space_item->Serialize());

          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          // The item should be immediately added to the model, and remain in
          // the persistent storage.
          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());
          persisted_holding_space_items_after_delayed_mount.Append(
              fresh_holding_space_item->Serialize());
          restored_holding_space_items.push_back(
              std::move(fresh_holding_space_item));
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value::ToUniquePtrValue(
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
    EXPECT_EQ(item->file_path(), item->file_path());
    // NOTE: `restored_item` was created with a fake file system URL (as it
    // could not be properly resolved at the time of item creation).
    EXPECT_EQ(item->file_system_url(),
              GetFileSystemUrl(secondary_profile, restored_item->file_path()));
  }

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_delayed_mount);
}

// Verifies that mounting volumes that contain no holding space items does not
// interfere with holding space restoration.
TEST_F(HoldingSpaceKeyedServiceTest,
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
  base::Value persisted_holding_space_items_after_restoration(
      base::Value::Type::LIST);
  base::Value persisted_holding_space_items_after_delayed_mount(
      base::Value::Type::LIST);

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value persisted_holding_space_items_before_restoration(
            base::Value::Type::LIST);

        // Persist some holding space items of each type.
        for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);
          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          // The item should be immediately added to the model, and remain in
          // the persistent storage.
          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());
          initialized_items_before_delayed_mount.push_back(
              fresh_holding_space_item->id());
          persisted_holding_space_items_after_restoration.Append(
              fresh_holding_space_item->Serialize());
          persisted_holding_space_items_after_delayed_mount.Append(
              fresh_holding_space_item->Serialize());
          restored_holding_space_items.push_back(
              std::move(fresh_holding_space_item));
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            base::Value::ToUniquePtrValue(
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
    if (item->IsInitialized())
      initialized_items.push_back(item->id());
  }
  EXPECT_EQ(initialized_items_before_delayed_mount, initialized_items);

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
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
    EXPECT_EQ(item->file_path(), item->file_path());
    // NOTE: `restored_item` was created with a fake file system URL (as it
    // could not be properly resolved at the time of item creation).
    EXPECT_EQ(item->file_system_url(),
              GetFileSystemUrl(secondary_profile, restored_item->file_path()));
  }

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_delayed_mount);
}

// Tests that items from an unmounted volume get removed from the holding space.
TEST_F(HoldingSpaceKeyedServiceTest, RemoveItemsFromUnmountedVolumes) {
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
  holding_space_service->AddScreenshot(file_path_1);

  const base::FilePath file_path_2 = test_mount_2->CreateArbitraryFile();
  holding_space_service->AddDownload(HoldingSpaceItem::Type::kDownload,
                                     file_path_2);

  const base::FilePath file_path_3 = test_mount_1->CreateArbitraryFile();
  holding_space_service->AddDownload(HoldingSpaceItem::Type::kDownload,
                                     file_path_3);

  EXPECT_EQ(3u, GetProfile()
                    ->GetPrefs()
                    ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                    ->GetListDeprecated()
                    .size());
  EXPECT_EQ(3u, holding_space_model->items().size());

  test_mount_1.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, GetProfile()
                    ->GetPrefs()
                    ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                    ->GetListDeprecated()
                    .size());
  ASSERT_EQ(1u, holding_space_model->items().size());
  EXPECT_EQ(file_path_2, holding_space_model->items()[0]->file_path());
}

// Verifies that files restored from persistence are not older than
// `kMaxFileAge`.
TEST_F(HoldingSpaceKeyedServiceTest, RemoveOlderFilesFromPersistence) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::Value persisted_holding_space_items_after_restoration(
      base::Value::Type::LIST);
  base::Time last_creation_time = base::Time::Now();

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Value persisted_holding_space_items_before_restoration(
            base::Value::Type::LIST);

        // Persist some holding space items of each type.
        for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
          const base::FilePath file = downloads_mount->CreateArbitraryFile();
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  base::BindOnce(&holding_space_util::ResolveImage,
                                 primary_holding_space_service
                                     ->thumbnail_loader_for_testing()));

          persisted_holding_space_items_before_restoration.Append(
              fresh_holding_space_item->Serialize());

          // Only pinned files are exempt from age checks. In this test, we
          // expect all holding space items of other types to be removed from
          // persistence during restoration due to being older than
          // `kMaxFileAge`.
          if (type == HoldingSpaceItem::Type::kPinnedFile) {
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
            base::Value::ToUniquePtrValue(
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
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);
}

TEST_F(HoldingSpaceKeyedServiceTest, AddArcDownloadItem) {
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
      /*relative_path=*/base::FilePath("Download.png"), /*content=*/"foo");

  // Simulate an event from ARC to indicate that the Android application with
  // package `com.bar.foo` added a download at `file_path`.
  auto* arc_intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
  ASSERT_TRUE(arc_intent_helper_bridge);
  arc_intent_helper_bridge->OnDownloadAdded(
      /*relative_path=*/"Download/Download.png",
      /*owner_package_name=*/"com.bar.foo");

  // Verify that an item of type `kArcDownload` was added to holding space.
  ASSERT_EQ(1u, model->items().size());
  const HoldingSpaceItem* arc_download_item = model->items()[0].get();
  EXPECT_EQ(arc_download_item->type(), HoldingSpaceItem::Type::kArcDownload);
  EXPECT_EQ(arc_download_item->file_path(),
            file_manager::util::GetDownloadsFolderForProfile(profile).Append(
                base::FilePath("Download.png")));
}

TEST_F(HoldingSpaceKeyedServiceTest, AddInProgressDownloadItem) {
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsIndeterminate());

  // Update the received bytes and total bytes for the download.
  current_received_bytes = 50;
  current_total_bytes = 100;
  UpdateFakeDownloadItem();

  // Verify that the holding space item has expected progress.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0]->type(), HoldingSpaceItem::Type::kDownload);
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
  EXPECT_EQ(model->items()[0]->progress().GetValue(), 0.5f);

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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
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

TEST_F(HoldingSpaceKeyedServiceTest, RemoveAll) {
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
  service->AddDownload(HoldingSpaceItem::Type::kDownload, download_path);
  service->AddPinnedFiles(
      {file_manager::util::GetFileManagerFileSystemContext(profile)
           ->CrackURLInFirstPartyContext(
               holding_space_util::ResolveFileSystemUrl(profile,
                                                        pinned_file_path))});

  ASSERT_EQ(2u, model->items().size());
  service->RemoveAll();
  EXPECT_EQ(0u, model->items().size());
}

TEST_F(HoldingSpaceKeyedServiceTest, CreateInterruptedDownloadItem) {
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsComplete());
}

TEST_F(HoldingSpaceKeyedServiceTest, InterruptAndResumeDownload) {
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
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
  EXPECT_EQ(model->items()[0]->file_path(), current_path);
  EXPECT_TRUE(model->items()[0]->progress().IsComplete());
}

// Base class for tests which verify adding items to holding space works as
// intended, parameterized by holding space item type.
class HoldingSpaceKeyedServiceAddItemTest
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
  // by the file at the specified absolute `file_path`.
  void AddItem(Profile* profile,
               HoldingSpaceItem::Type type,
               const base::FilePath& file_path) {
    auto* const holding_space_service = GetService(profile);
    ASSERT_TRUE(holding_space_service);
    const auto* holding_space_model =
        holding_space_service->model_for_testing();
    ASSERT_TRUE(holding_space_model);

    switch (type) {
      case HoldingSpaceItem::Type::kArcDownload:
      case HoldingSpaceItem::Type::kDownload:
      case HoldingSpaceItem::Type::kLacrosDownload:
        EXPECT_EQ(holding_space_model->ContainsItem(type, file_path),
                  holding_space_service->AddDownload(type, file_path).empty());
        break;
      case HoldingSpaceItem::Type::kDiagnosticsLog:
        holding_space_service->AddDiagnosticsLog(file_path);
        break;
      case HoldingSpaceItem::Type::kNearbyShare:
        holding_space_service->AddNearbyShare(file_path);
        break;
      case HoldingSpaceItem::Type::kPinnedFile:
        holding_space_service->AddPinnedFiles(
            {file_manager::util::GetFileManagerFileSystemContext(profile)
                 ->CrackURLInFirstPartyContext(
                     holding_space_util::ResolveFileSystemUrl(profile,
                                                              file_path))});
        break;
      case HoldingSpaceItem::Type::kPrintedPdf:
        holding_space_service->AddPrintedPdf(file_path,
                                             /*from_incognito_profile=*/false);
        break;
      case HoldingSpaceItem::Type::kScan:
        holding_space_service->AddScan(file_path);
        break;
      case HoldingSpaceItem::Type::kScreenRecording:
        holding_space_service->AddScreenRecording(file_path);
        break;
      case HoldingSpaceItem::Type::kScreenshot:
        holding_space_service->AddScreenshot(file_path);
        break;
      case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
        EXPECT_EQ(
            holding_space_model->ContainsItem(type, file_path),
            holding_space_service
                ->AddPhoneHubCameraRollItem(file_path, HoldingSpaceProgress())
                .empty());
        break;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceKeyedServiceAddItemTest,
                         ::testing::ValuesIn(GetHoldingSpaceItemTypes()));

TEST_P(HoldingSpaceKeyedServiceAddItemTest, AddItem) {
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
  AddItem(profile, GetType(), file_path);

  // Verify a holding space item has been added to the model.
  ASSERT_EQ(model->items().size(), 1u);

  // Verify holding space `item` metadata.
  HoldingSpaceItem* const item = model->items()[0].get();
  EXPECT_EQ(item->type(), GetType());
  EXPECT_EQ(item->GetText(), file_path.BaseName().LossyDisplayName());
  EXPECT_EQ(item->file_path(), file_path);
  EXPECT_EQ(item->file_system_url(),
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
  AddItem(profile, GetType(), file_path);

  // Attempts to add already represented items should be ignored.
  ASSERT_EQ(model->items().size(), 1u);
  EXPECT_EQ(model->items()[0].get(), item);
}

TEST_P(HoldingSpaceKeyedServiceAddItemTest, AddItemOfType) {
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
  EXPECT_FALSE(id.empty());

  // Verify a holding space item has been added to the model.
  ASSERT_EQ(model->items().size(), 1u);

  // Verify holding space `item` metadata.
  HoldingSpaceItem* const item = model->items()[0].get();
  EXPECT_EQ(item->id(), id);
  EXPECT_EQ(item->type(), GetType());
  EXPECT_EQ(item->GetText(), file_path.BaseName().LossyDisplayName());
  EXPECT_EQ(item->file_path(), file_path);
  EXPECT_EQ(item->file_system_url(),
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

  holding_space_service->AddNearbyShare(item_1_full_path);

  const base::FilePath item_2_virtual_path = base::FilePath("Alt/File 2.png");
  // Create a fake nearby shared file on the local file system - later parts of
  // the test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_2_full_path =
      downloads_mount->CreateFile(item_2_virtual_path, "blue");
  ASSERT_FALSE(item_2_full_path.empty());
  holding_space_service->AddNearbyShare(item_2_full_path);

  EXPECT_EQ(initial_model, HoldingSpaceController::Get()->model());
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            holding_space_service->model_for_testing());

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(2u, model->items().size());

  const HoldingSpaceItem* item_1 = model->items()[0].get();
  EXPECT_EQ(item_1_full_path, item_1->file_path());
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
            GetVirtualPathFromUrl(item_1->file_system_url(),
                                  downloads_mount->name()));
  EXPECT_EQ(u"File 1.png", item_1->GetText());

  const HoldingSpaceItem* item_2 = model->items()[1].get();
  EXPECT_EQ(item_2_full_path, item_2->file_path());
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
            GetVirtualPathFromUrl(item_2->file_system_url(),
                                  downloads_mount->name()));
  EXPECT_EQ(u"File 2.png", item_2->GetText());
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

    std::string data;
    pdf_printer_handler_->StartPrint(job_title,
                                     /*settings=*/base::Value::Dict(),
                                     base::RefCountedString::TakeString(&data),
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
    if (!UseIncognitoBrowser())
      return browser();
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
  EXPECT_EQ(model->items()[0]->file_path(), file_path);
}

// Base class for tests of incognito profile integration.
class HoldingSpaceKeyedServiceIncognitoDownloadsTest
    : public HoldingSpaceKeyedServiceTest {
 public:
  // HoldingSpaceKeyedServiceTest:
  TestingProfile* CreateProfile() override {
    TestingProfile* profile = HoldingSpaceKeyedServiceTest::CreateProfile();

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
  TestingProfile* incognito_profile_ = nullptr;
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
  EXPECT_EQ(download_item->file_path(), current_path);
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
  EXPECT_EQ(download_item->file_path(), current_path);
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
  EXPECT_EQ(download_item->file_path(), current_path);
  EXPECT_FALSE(download_item->progress().IsComplete());

  // Verify that destroying a profile with an in-progress download destroys
  // the holding space item.
  profile->DestroyOffTheRecordProfile(incognito_profile());
  ASSERT_EQ(0u, model->items().size());
}

}  // namespace ash
