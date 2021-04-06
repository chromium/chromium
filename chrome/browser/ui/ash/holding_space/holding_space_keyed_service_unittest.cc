// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

using holding_space::ScopedTestMountPoint;

namespace {

// Creates an empty holding space image.
std::unique_ptr<HoldingSpaceImage> CreateTestHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      HoldingSpaceImage::GetMaxSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.push_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */,
      chromeos::disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

// Utility class which can wait until a `HoldingSpaceModel` for a given profile
// is attached to the `HoldingSpaceController`.
class HoldingSpaceModelAttachedWaiter : public HoldingSpaceControllerObserver {
 public:
  explicit HoldingSpaceModelAttachedWaiter(Profile* profile)
      : profile_(profile) {
    holding_space_controller_observer_.Add(HoldingSpaceController::Get());
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
  ScopedObserver<HoldingSpaceController, HoldingSpaceControllerObserver>
      holding_space_controller_observer_{this};
  std::unique_ptr<base::RunLoop> wait_loop_;
};

class ItemUpdatedWaiter : public HoldingSpaceModelObserver {
 public:
  explicit ItemUpdatedWaiter(HoldingSpaceModel* model) {
    model_observer_.Observe(model);
  }

  ItemUpdatedWaiter(const ItemUpdatedWaiter&) = delete;
  ItemUpdatedWaiter& operator=(const ItemUpdatedWaiter&) = delete;
  ~ItemUpdatedWaiter() override = default;

  void Wait(const HoldingSpaceItem* item) {
    ASSERT_FALSE(wait_item_);
    ASSERT_FALSE(wait_loop_);

    wait_item_ = item;

    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();

    wait_item_ = nullptr;
  }

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override {
    if (item == wait_item_)
      wait_loop_->Quit();
  }

  const HoldingSpaceItem* wait_item_ = nullptr;
  std::unique_ptr<base::RunLoop> wait_loop_;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};
};

class ItemsFinalizedWaiter : public HoldingSpaceModelObserver {
 public:
  // Predicate that determines whether the waiter should wait for an item to be
  // finalized.
  using ItemFilter =
      base::RepeatingCallback<bool(const HoldingSpaceItem* item)>;

  explicit ItemsFinalizedWaiter(HoldingSpaceModel* model) : model_(model) {}
  ItemsFinalizedWaiter(const ItemsFinalizedWaiter&) = delete;
  ItemsFinalizedWaiter& operator=(const ItemsFinalizedWaiter&) = delete;
  ~ItemsFinalizedWaiter() override = default;

  // NOTE: The filter defaults to all items.
  void Wait(const ItemFilter& filter = ItemFilter()) {
    ASSERT_FALSE(wait_loop_);

    filter_ = filter;
    if (FilteredItemsFinalized())
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
    if (FilteredItemsFinalized())
      wait_loop_->Quit();
  }

  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override {
    if (FilteredItemsFinalized())
      wait_loop_->Quit();
  }

 private:
  bool FilteredItemsFinalized() const {
    for (auto& item : model_->items()) {
      if (filter_ && !filter_.Run(item.get()))
        continue;
      if (!item->IsFinalized())
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
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)),
        download_manager_(
            std::make_unique<testing::NiceMock<MockDownloadManager>>()) {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
    HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(true);
  }

  HoldingSpaceKeyedServiceTest(const HoldingSpaceKeyedServiceTest& other) =
      delete;
  HoldingSpaceKeyedServiceTest& operator=(
      const HoldingSpaceKeyedServiceTest& other) = delete;
  ~HoldingSpaceKeyedServiceTest() override {
    HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(false);
  }

  TestingProfile* CreateProfile() override {
    const std::string kPrimaryProfileName = "primary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));

    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    GetSessionControllerClient()->AddUserSession(kPrimaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);

    return profile_manager()->CreateTestingProfile(
        kPrimaryProfileName,
        /*testing_factories=*/{
            {file_manager::VolumeManagerFactory::GetInstance(),
             base::BindRepeating(&BuildVolumeManager)}});
  }

  TestingProfile* CreateSecondaryProfile(
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs = nullptr) {
    const std::string kSecondaryProfileName = "secondary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    return profile_manager()->CreateTestingProfile(
        kSecondaryProfileName, std::move(prefs), u"Test profile",
        1 /*avatar_id*/, std::string() /*supervised_user_id*/,
        /*testing_factories=*/
        {{file_manager::VolumeManagerFactory::GetInstance(),
          base::BindRepeating(&BuildVolumeManager)}});
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
        profile, absolute_file_path, file_manager::kFileManagerAppId,
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
        file_manager::util::GetFileSystemContextForExtensionId(
            GetProfile(), file_manager::kFileManagerAppId);
    storage::FileSystemURL fs_url = fs_context->CrackURL(url);

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

  std::unique_ptr<download::MockDownloadItem> CreateMockDownloadItem(
      base::FilePath full_file_path) {
    auto item =
        std::make_unique<testing::NiceMock<download::MockDownloadItem>>();
    ON_CALL(*item, GetId()).WillByDefault(testing::Return(1));
    ON_CALL(*item, GetGuid())
        .WillByDefault(testing::ReturnRefOfCopy(
            std::string("14CA04AF-ECEC-4B13-8829-817477EFAB83")));
    ON_CALL(*item, GetFullPath())
        .WillByDefault(testing::ReturnRefOfCopy(full_file_path));
    ON_CALL(*item, GetURL())
        .WillByDefault(testing::ReturnRefOfCopy(GURL("foo/bar")));
    ON_CALL(*item, GetMimeType()).WillByDefault(testing::Return(std::string()));
    content::DownloadItemUtils::AttachInfo(item.get(), GetProfile(), nullptr);
    return item;
  }

  MockDownloadManager* download_manager() { return download_manager_.get(); }

 private:
  // BrowserWithTestWindowTest:
  void SetUp() override {
    // Needed by `file_manager::VolumeManager`.
    chromeos::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager);
    SetUpDownloadManager();
    BrowserWithTestWindowTest::SetUp();
    holding_space_util::SetNowForTesting(base::nullopt);
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    chromeos::disks::DiskMountManager::Shutdown();
  }

  void SetUpDownloadManager() {
    // The `content::DownloadManager` needs to be set prior to initialization
    // of the `HoldingSpaceDownloadsDelegate`. This must happen before the
    // `HoldingSpaceKeyedService` is created for the profile under test.
    MockDownloadManager* mock_download_manager = download_manager();
    HoldingSpaceDownloadsDelegate::SetDownloadManagerForTesting(
        mock_download_manager);

    // Spoof initialization of the `mock_download_manager`.
    ON_CALL(*mock_download_manager, IsManagerInitialized)
        .WillByDefault(testing::Return(true));
  }

  FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::unique_ptr<MockDownloadManager> download_manager_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests adding a screenshot item. Verifies that adding a screenshot creates a
// holding space item with a file system URL that can be accessed by the file
// manager app.
TEST_F(HoldingSpaceKeyedServiceTest, AddScreenshotItem) {
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
  const base::FilePath item_1_virtual_path("Screenshot 1.png");
  // Create a fake screenshot file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_1_full_path =
      downloads_mount->CreateFile(item_1_virtual_path, "red");
  ASSERT_FALSE(item_1_full_path.empty());

  holding_space_service->AddScreenshot(item_1_full_path);

  const base::FilePath item_2_virtual_path =
      base::FilePath("Alt/Screenshot 2.png");
  // Create a fake screenshot file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_2_full_path =
      downloads_mount->CreateFile(item_2_virtual_path, "blue");
  ASSERT_FALSE(item_2_full_path.empty());
  holding_space_service->AddScreenshot(item_2_full_path);

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
           HoldingSpaceItem::Type::kScreenshot, item_1_full_path)
           ->GetImageSkia()
           .bitmap(),
      *item_1->image().GetImageSkia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(item_1_virtual_path,
            GetVirtualPathFromUrl(item_1->file_system_url(),
                                  downloads_mount->name()));
  EXPECT_EQ(u"Screenshot 1.png", item_1->text());

  const HoldingSpaceItem* item_2 = model->items()[1].get();
  EXPECT_EQ(item_2_full_path, item_2->file_path());
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           holding_space_service->thumbnail_loader_for_testing(),
           HoldingSpaceItem::Type::kScreenshot, item_2_full_path)
           ->GetImageSkia()
           .bitmap(),
      *item_2->image().GetImageSkia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(item_2_virtual_path,
            GetVirtualPathFromUrl(item_2->file_system_url(),
                                  downloads_mount->name()));
  EXPECT_EQ(u"Screenshot 2.png", item_2->text());

  // Attempt to add an already added screenshot to the model.
  EXPECT_EQ(model->items()[1]->file_path(), item_2_full_path);
  holding_space_service->AddScreenshot(item_2_full_path);

  // Attempts to add already added screenshots should be ignored.
  ASSERT_EQ(model->items().size(), 2u);
  EXPECT_EQ(model->items()[0].get(), item_1);
  EXPECT_EQ(model->items()[1].get(), item_2);
}

TEST_F(HoldingSpaceKeyedServiceTest, GuestUserProfile) {
  // Construct a guest session profile.
  TestingProfile::Builder guest_profile_builder;
  guest_profile_builder.SetGuestSession();
  guest_profile_builder.SetProfileName("guest_profile");
  guest_profile_builder.AddTestingFactories(
      {{file_manager::VolumeManagerFactory::GetInstance(),
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
  ASSERT_TRUE(guest_profile->GetPrimaryOTRProfile());
  HoldingSpaceKeyedService* const primary_otr_guest_profile_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          guest_profile->GetPrimaryOTRProfile());
  ASSERT_EQ(guest_profile_service, primary_otr_guest_profile_service);

  // Construct a second OTR profile from `guest_profile`.
  TestingProfile::Builder secondary_otr_guest_profile_builder;
  secondary_otr_guest_profile_builder.SetGuestSession();
  secondary_otr_guest_profile_builder.SetProfileName(
      guest_profile->GetProfileUserName());
  TestingProfile* const secondary_otr_guest_profile =
      secondary_otr_guest_profile_builder.BuildOffTheRecord(
          guest_profile.get(), Profile::OTRProfileID("profile::secondary_otr"));
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
            primary_holding_space_service->client_for_testing());
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            primary_holding_space_service->model_for_testing());

  // Switching the active user should change the active client/model (multi-user
  // support).
  ActivateSecondaryProfile();
  EXPECT_EQ(HoldingSpaceController::Get()->client(),
            secondary_holding_space_service->client_for_testing());
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

  base::ListValue persisted_holding_space_items;

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

    persisted_holding_space_items.Remove(0, /*out_value=*/nullptr);
    primary_holding_space_model->RemoveItem(holding_space_item->id());

    EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
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
      file_manager::util::GetFileSystemContextForExtensionId(
          GetProfile(), file_manager::kFileManagerAppId);
  ASSERT_TRUE(context);

  base::ListValue persisted_holding_space_items;

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
    ASSERT_EQ(storage::AsyncFileTestHelper::Move(
                  context, context->CrackURL(file_path_url),
                  context->CrackURL(new_file_path_url)),
              base::File::FILE_OK);

    // File changes must be posted to the UI thread, wait for the update to
    // reach the holding space model.
    ItemUpdatedWaiter(primary_holding_space_model).Wait(holding_space_item);

    // Verify that the holding space item has been updated in place.
    ASSERT_EQ(holding_space_item->file_path(), new_file_path);
    ASSERT_EQ(holding_space_item->file_system_url(), new_file_path_url);
    ASSERT_EQ(holding_space_item->text(),
              new_file_path.BaseName().LossyDisplayName());

    // Verify that persistence has been updated.
    persisted_holding_space_items.GetList()[i] =
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
    ASSERT_EQ(storage::AsyncFileTestHelper::Move(
                  context, context->CrackURL(file_path_url),
                  context->CrackURL(new_file_path_url)),
              base::File::FILE_OK);

    // File changes must be posted to the UI thread, wait for the update to
    // reach the holding space model.
    ItemUpdatedWaiter(primary_holding_space_model).Wait(holding_space_item);

    // The file backing the holding space item is expected to have re-parented.
    new_file_path = new_file_path.Append(base_name);
    new_file_path_url = GetFileSystemUrl(GetProfile(), new_file_path);

    // Verify that the holding space item has been updated in place.
    ASSERT_EQ(holding_space_item->file_path(), new_file_path);
    ASSERT_EQ(holding_space_item->file_system_url(), new_file_path_url);
    ASSERT_EQ(holding_space_item->text(),
              new_file_path.BaseName().LossyDisplayName());

    // Verify that persistence has been updated.
    persisted_holding_space_items.GetList()[i] =
        holding_space_item->Serialize();
    ASSERT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
}

// Tests that holding space item's image representation gets updated when the
// backing file is changed using move operation. Furthermore, verifies that
// conflicts caused by moving a holding space item file to another path present
// in the holding space get resolved.
TEST_F(HoldingSpaceKeyedServiceTest, UpdateItemsOverwrittenByMove) {
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
      file_manager::util::GetFileSystemContextForExtensionId(
          GetProfile(), file_manager::kFileManagerAppId);
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

  base::ListValue persisted_holding_space_items;

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

  base::ListValue final_persisted_holding_space_items;
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
    ASSERT_EQ(storage::AsyncFileTestHelper::Move(
                  context,
                  context->CrackURL(GetFileSystemUrl(
                      GetProfile(), path_not_in_holding_space)),
                  context->CrackURL(src_item->file_system_url())),
              base::File::FILE_OK);

    image_update_waiter.Wait();

    ASSERT_EQ(src_item,
              primary_holding_space_model->GetItem(test_case.src.item_id));
    EXPECT_TRUE(primary_holding_space_model->GetItem(test_case.dst.item_id));

    ASSERT_EQ(src_item->file_path(), test_case.src.path);
    ASSERT_EQ(src_item->file_system_url(), test_case.src.file_system_url);

    // Move the file at the source item path to the destination item path.
    // Verify that, given that both paths are represented in the holding space,
    // the item initially associated with the destination path is removed from
    // the holding space (to avoid two items with the same backing file).
    ASSERT_EQ(storage::AsyncFileTestHelper::Move(
                  context, context->CrackURL(test_case.src.file_system_url),
                  context->CrackURL(test_case.dst.file_system_url)),
              base::File::FILE_OK);

    // File changes must be posted to the UI thread, wait for the update to
    // reach the holding space model.
    ItemUpdatedWaiter(primary_holding_space_model).Wait(src_item);

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
  base::ListValue persisted_holding_space_items_after_restoration;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        auto persisted_holding_space_items_before_restoration =
            std::make_unique<base::ListValue>();

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

          persisted_holding_space_items_before_restoration->Append(
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
          persisted_holding_space_items_before_restoration->Append(
              stale_holding_space_item->Serialize());
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            std::move(persisted_holding_space_items_before_restoration),
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

  ItemsFinalizedWaiter(secondary_holding_space_model).Wait();
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

  std::vector<std::string> finalized_items_before_delayed_mount;
  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::ListValue persisted_holding_space_items_after_restoration;
  base::ListValue persisted_holding_space_items_after_delayed_mount;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        auto persisted_holding_space_items_before_restoration =
            std::make_unique<base::ListValue>();

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
          persisted_holding_space_items_before_restoration->Append(
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
          persisted_holding_space_items_before_restoration->Append(
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
          persisted_holding_space_items_before_restoration->Append(
              fresh_holding_space_item->Serialize());
          finalized_items_before_delayed_mount.push_back(
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
            std::move(persisted_holding_space_items_before_restoration),
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

  ItemsFinalizedWaiter(secondary_holding_space_model)
      .Wait(
          /*filter=*/base::BindLambdaForTesting(
              [&downloads_mount](const HoldingSpaceItem* item) -> bool {
                return downloads_mount->GetRootPath().IsParent(
                    item->file_path());
              }));

  std::vector<std::string> finalized_items;
  for (const auto& item : secondary_holding_space_model->items()) {
    if (item->IsFinalized())
      finalized_items.push_back(item->id());
  }
  EXPECT_EQ(finalized_items_before_delayed_mount, finalized_items);

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);

  delayed_mount->CreateFile(delayed_mount_file_name, "fake");
  delayed_mount->Mount(secondary_profile);

  ItemsFinalizedWaiter(secondary_holding_space_model).Wait();

  EXPECT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    SCOPED_TRACE(testing::Message() << "Item at index " << i);

    EXPECT_TRUE(item->IsFinalized());

    EXPECT_EQ(item->id(), restored_item->id());
    EXPECT_EQ(item->type(), restored_item->type());
    EXPECT_EQ(item->text(), restored_item->text());
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
  base::ListValue persisted_holding_space_items_after_delayed_mount;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        auto persisted_holding_space_items_before_restoration =
            std::make_unique<base::ListValue>();

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
          persisted_holding_space_items_before_restoration->Append(
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
          persisted_holding_space_items_before_restoration->Append(
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
          persisted_holding_space_items_before_restoration->Append(
              fresh_holding_space_item->Serialize());
          persisted_holding_space_items_after_delayed_mount.Append(
              fresh_holding_space_item->Serialize());
          restored_holding_space_items.push_back(
              std::move(fresh_holding_space_item));
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            std::move(persisted_holding_space_items_before_restoration),
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

  ItemsFinalizedWaiter(secondary_holding_space_model).Wait();
  ASSERT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    SCOPED_TRACE(testing::Message() << "Item at index " << i);

    EXPECT_TRUE(item->IsFinalized());

    EXPECT_EQ(item->id(), restored_item->id());
    EXPECT_EQ(item->type(), restored_item->type());
    EXPECT_EQ(item->text(), restored_item->text());
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

  std::vector<std::string> finalized_items_before_delayed_mount;
  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::ListValue persisted_holding_space_items_after_restoration;
  base::ListValue persisted_holding_space_items_after_delayed_mount;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        auto persisted_holding_space_items_before_restoration =
            std::make_unique<base::ListValue>();

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
          persisted_holding_space_items_before_restoration->Append(
              fresh_holding_space_item->Serialize());
          finalized_items_before_delayed_mount.push_back(
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
            std::move(persisted_holding_space_items_before_restoration),
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

  ItemsFinalizedWaiter(secondary_holding_space_model).Wait();

  std::vector<std::string> finalized_items;
  for (const auto& item : secondary_holding_space_model->items()) {
    if (item->IsFinalized())
      finalized_items.push_back(item->id());
  }
  EXPECT_EQ(finalized_items_before_delayed_mount, finalized_items);

  // Verify persisted holding space items.
  EXPECT_EQ(*secondary_profile->GetPrefs()->GetList(
                HoldingSpacePersistenceDelegate::kPersistencePath),
            persisted_holding_space_items_after_restoration);

  delayed_mount_2->Mount(secondary_profile);
  ItemsFinalizedWaiter(secondary_holding_space_model).Wait();

  EXPECT_EQ(secondary_holding_space_model->items().size(),
            restored_holding_space_items.size());

  // Verify in-memory holding space items.
  for (size_t i = 0; i < secondary_holding_space_model->items().size(); ++i) {
    const auto& item = secondary_holding_space_model->items()[i];
    const auto& restored_item = restored_holding_space_items[i];
    SCOPED_TRACE(testing::Message() << "Item at index " << i);

    EXPECT_TRUE(item->IsFinalized());

    EXPECT_EQ(item->id(), restored_item->id());
    EXPECT_EQ(item->type(), restored_item->type());
    EXPECT_EQ(item->text(), restored_item->text());
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
  holding_space_service->AddDownload(file_path_2);

  const base::FilePath file_path_3 = test_mount_1->CreateArbitraryFile();
  holding_space_service->AddDownload(file_path_3);

  EXPECT_EQ(3u, GetProfile()
                    ->GetPrefs()
                    ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                    ->GetList()
                    .size());
  EXPECT_EQ(3u, holding_space_model->items().size());

  test_mount_1.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, GetProfile()
                    ->GetPrefs()
                    ->GetList(HoldingSpacePersistenceDelegate::kPersistencePath)
                    ->GetList()
                    .size());
  ASSERT_EQ(1u, holding_space_model->items().size());
  EXPECT_EQ(file_path_2, holding_space_model->items()[0]->file_path());
}

// Verifies that screenshots restored from persistence are not older than
// kMaxFileAge.
TEST_F(HoldingSpaceKeyedServiceTest, RemoveOlderFilesFromPersistance) {
  // Create file system mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(GetProfile());
  ASSERT_TRUE(downloads_mount->IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  HoldingSpaceModel::ItemList restored_holding_space_items;
  base::ListValue persisted_holding_space_items_after_restoration;

  // Create a secondary profile w/ a pre-populated pref store.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        auto persisted_holding_space_items_before_restoration =
            std::make_unique<base::ListValue>();

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

          persisted_holding_space_items_before_restoration->Append(
              fresh_holding_space_item->Serialize());

          // Only pinned files are exempt from age checks. In this test, we
          // expect all holding space items of other types to be removed from
          // persistence during restoration due to being older than kMaxFileAge.
          if (type == HoldingSpaceItem::Type::kPinnedFile) {
            persisted_holding_space_items_after_restoration.Append(
                fresh_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }

          const base::FilePath stale_item_file =
              downloads_mount->GetRootPath().AppendASCII(
                  base::UnguessableToken::Create().ToString());
          auto stale_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, stale_item_file,
                  GetFileSystemUrl(GetProfile(), stale_item_file),
                  base::BindOnce(&CreateTestHoldingSpaceImage));

          // NOTE: While the `stale_holding_space_item` is persisted here, we do
          // *not* expect it to be restored or to be persisted after model
          // restoration since its backing file does *not* exist.
          persisted_holding_space_items_before_restoration->Append(
              stale_holding_space_item->Serialize());
        }

        pref_store->SetValueSilently(
            HoldingSpacePersistenceDelegate::kPersistencePath,
            std::move(persisted_holding_space_items_before_restoration),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));

  holding_space_util::SetNowForTesting(base::Time::Now() + kMaxFileAge);

  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          secondary_profile);
  HoldingSpaceModel* const secondary_holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  ItemsFinalizedWaiter(secondary_holding_space_model).Wait();

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

TEST_F(HoldingSpaceKeyedServiceTest, AddDownloadItem) {
  TestingProfile* profile = GetProfile();
  HoldingSpaceModelAttachedWaiter(profile).Wait();

  // Create a test downloads mount point.
  std::unique_ptr<ScopedTestMountPoint> downloads_mount =
      ScopedTestMountPoint::CreateAndMountDownloads(profile);
  ASSERT_TRUE(downloads_mount->IsValid());

  // Create a fake download file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath download_item_virtual_path("Download 1.png");
  const base::FilePath download_item_full_path =
      downloads_mount->CreateFile(download_item_virtual_path, "download 1");

  MockDownloadManager* mock_download_manager = download_manager();
  std::unique_ptr<download::MockDownloadItem> item(
      CreateMockDownloadItem(download_item_full_path));

  download::MockDownloadItem* mock_download_item = item.get();
  EXPECT_CALL(*mock_download_manager, MockCreateDownloadItem(testing::_))
      .WillRepeatedly(testing::DoAll(
          testing::InvokeWithoutArgs([mock_download_manager,
                                      mock_download_item]() {
            mock_download_manager->NotifyDownloadCreated(mock_download_item);
          }),
          testing::Return(item.get())));

  std::vector<GURL> url_chain;
  url_chain.push_back(item->GetURL());
  mock_download_manager->CreateDownloadItem(
      base::GenerateGUID(), item->GetId(), item->GetFullPath(),
      item->GetFullPath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      url::Origin(), item->GetMimeType(), item->GetMimeType(),
      base::Time::Now(), base::Time::Now(), "", "", 10, 10, "",
      download::DownloadItem::IN_PROGRESS,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      download::DOWNLOAD_INTERRUPT_REASON_NONE, false, base::Time::Now(), false,
      std::vector<download::DownloadItem::ReceivedSlice>());

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());

  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(testing::Return(download::DownloadItem::IN_PROGRESS));
  item->NotifyObserversDownloadUpdated();

  ASSERT_EQ(0u, model->items().size());

  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(testing::Return(download::DownloadItem::COMPLETE));
  item->NotifyObserversDownloadUpdated();

  ASSERT_EQ(1u, model->items().size());

  const HoldingSpaceItem* download_item = model->items()[0].get();
  EXPECT_EQ(download_item_full_path, download_item->file_path());
  EXPECT_EQ(download_item_virtual_path,
            GetVirtualPathFromUrl(download_item->file_system_url(),
                                  downloads_mount->name()));
}

class HoldingSpaceKeyedServiceNearbySharingTest
    : public HoldingSpaceKeyedServiceTest {
 public:
  HoldingSpaceKeyedServiceNearbySharingTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kNearbySharing, ash::features::kTemporaryHoldingSpace},
        {});
  }

  ~HoldingSpaceKeyedServiceNearbySharingTest() override = default;

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
  EXPECT_EQ(u"File 1.png", item_1->text());

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
  EXPECT_EQ(u"File 2.png", item_2->text());
}

TEST_F(HoldingSpaceKeyedServiceTest, AddScreenRecordingItem) {
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
  const base::FilePath item_1_virtual_path("Screen Recording 1.mpg");
  // Create some fake screen recording file on the local file system - later
  // parts of the test will try to resolve the file's file system URL, which
  // fails if the file does not exist.
  const base::FilePath item_1_full_path =
      downloads_mount->CreateFile(item_1_virtual_path, "recording 1");
  ASSERT_FALSE(item_1_full_path.empty());

  holding_space_service->AddScreenRecording(item_1_full_path);

  const base::FilePath item_2_virtual_path =
      base::FilePath("Alt/Screen Recording 2.mpg");
  const base::FilePath item_2_full_path =
      downloads_mount->CreateFile(item_2_virtual_path, "recording 2");
  ASSERT_FALSE(item_2_full_path.empty());
  holding_space_service->AddScreenRecording(item_2_full_path);

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
           HoldingSpaceItem::Type::kScreenRecording, item_1_full_path)
           ->GetImageSkia()
           .bitmap(),
      *item_1->image().GetImageSkia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(item_1_virtual_path,
            GetVirtualPathFromUrl(item_1->file_system_url(),
                                  downloads_mount->name()));
  EXPECT_EQ(u"Screen Recording 1.mpg", item_1->text());

  const HoldingSpaceItem* item_2 = model->items()[1].get();
  EXPECT_EQ(item_2_full_path, item_2->file_path());
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           holding_space_service->thumbnail_loader_for_testing(),
           HoldingSpaceItem::Type::kScreenRecording, item_2_full_path)
           ->GetImageSkia()
           .bitmap(),
      *item_2->image().GetImageSkia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(item_2_virtual_path,
            GetVirtualPathFromUrl(item_2->file_system_url(),
                                  downloads_mount->name()));
  EXPECT_EQ(u"Screen Recording 2.mpg", item_2->text());

  // Attempt to add an item with an empty file. Verify nothing gets added to the
  // model.
  const base::FilePath item_3_virtual_path = base::FilePath("");
  const base::FilePath item_3_full_path =
      downloads_mount->CreateFile(item_3_virtual_path, "");
  ASSERT_TRUE(item_3_full_path.empty());
  holding_space_service->AddScreenRecording(item_3_full_path);

  ASSERT_EQ(2u, model->items().size());

  // Attempt to add an already added screen capture to the model.
  EXPECT_EQ(model->items()[1]->file_path(), item_2_full_path);
  holding_space_service->AddScreenRecording(item_2_full_path);

  // Attempts to add already added screen capture should be ignored.
  ASSERT_EQ(model->items().size(), 2u);
  EXPECT_EQ(model->items()[0].get(), item_1);
  EXPECT_EQ(model->items()[1].get(), item_2);
}

}  // namespace ash
