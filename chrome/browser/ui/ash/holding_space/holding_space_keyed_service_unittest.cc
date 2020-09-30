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
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

std::vector<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::vector<HoldingSpaceItem::Type> types;
  for (int i = 0; i < static_cast<int>(HoldingSpaceItem::Type::kMaxValue); ++i)
    types.push_back(static_cast<HoldingSpaceItem::Type>(i));
  return types;
}

// Utility class that registers the downloads external file system mount point,
// and grants file manager app access permission for the mount point.
class ScopedDownloadsMountPoint {
 public:
  explicit ScopedDownloadsMountPoint(Profile* profile)
      : name_(file_manager::util::GetDownloadsMountPointName(profile)) {
    if (!temp_dir_.CreateUniqueTempDir())
      return;

    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        name_, storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), temp_dir_.GetPath());
    file_manager::util::GetFileSystemContextForExtensionId(
        profile, file_manager::kFileManagerAppId)
        ->external_backend()
        ->GrantFileAccessToExtension(file_manager::kFileManagerAppId,
                                     base::FilePath(name_));
  }

  ~ScopedDownloadsMountPoint() {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
  }

  bool IsValid() const { return temp_dir_.IsValid(); }

  const base::FilePath& GetRootPath() const { return temp_dir_.GetPath(); }

  const std::string& name() const { return name_; }

 private:
  base::ScopedTempDir temp_dir_;
  std::string name_;
};

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
      : fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)),
        download_manager_(
            std::make_unique<testing::NiceMock<MockDownloadManager>>()) {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
  }

  HoldingSpaceKeyedServiceTest(const HoldingSpaceKeyedServiceTest& other) =
      delete;
  HoldingSpaceKeyedServiceTest& operator=(
      const HoldingSpaceKeyedServiceTest& other) = delete;
  ~HoldingSpaceKeyedServiceTest() override = default;

  TestingProfile* CreateProfile() override {
    const std::string kPrimaryProfileName = "primary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));

    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    GetSessionControllerClient()->AddUserSession(kPrimaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);

    return profile_manager()->CreateTestingProfile(kPrimaryProfileName);
  }

  TestingProfile* CreateSecondaryProfile(
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs = nullptr) {
    const std::string kSecondaryProfileName = "secondary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    return profile_manager()->CreateTestingProfile(
        kSecondaryProfileName, std::move(prefs),
        base::ASCIIToUTF16("Test profile"), 1 /*avatar_id*/,
        std::string() /*supervised_user_id*/,
        TestingProfile::TestingFactories());
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

  // Creates a file under path |mount_point|/|relative_path| with the provided
  // content. Returns the created file's file path, or an empty path on failure.
  base::FilePath CreateFile(const ScopedDownloadsMountPoint& mount_point,
                            const base::FilePath& relative_path,
                            const std::string& content) {
    const base::FilePath path = mount_point.GetRootPath().Append(relative_path);
    if (!base::CreateDirectory(path.DirName()))
      return base::FilePath();
    if (!base::WriteFile(path, content))
      return base::FilePath();
    return path;
  }

  // Creates an arbitrary file under the specified 'mount_point'.
  base::FilePath CreateArbitraryFile(
      const ScopedDownloadsMountPoint& mount_point) {
    return CreateFile(
        mount_point,
        base::FilePath(base::UnguessableToken::Create().ToString()),
        /*content=*/std::string());
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
    SetUpDownloadManager();
    BrowserWithTestWindowTest::SetUp();
    holding_space_util::SetNowForTesting(base::nullopt);
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

  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::unique_ptr<MockDownloadManager> download_manager_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests adding a screenshot item. Verifies that adding a screenshot creates a
// holding space item with a file system URL that can be accessed by the file
// manager app.
TEST_F(HoldingSpaceKeyedServiceTest, AddScreenshotItem) {
  // Create a test downloads mount point.
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

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
      CreateFile(downloads_mount, item_1_virtual_path, "red");
  ASSERT_FALSE(item_1_full_path.empty());

  holding_space_service->AddScreenshot(item_1_full_path);

  const base::FilePath item_2_virtual_path =
      base::FilePath("Alt/Screenshot 2.png");
  // Create a fake screenshot file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_2_full_path =
      CreateFile(downloads_mount, item_2_virtual_path, "blue");
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
           ->image_skia()
           .bitmap(),
      *item_1->image().image_skia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(
      item_1_virtual_path,
      GetVirtualPathFromUrl(item_1->file_system_url(), downloads_mount.name()));
  EXPECT_EQ(base::ASCIIToUTF16("Screenshot 1.png"), item_1->text());

  const HoldingSpaceItem* item_2 = model->items()[1].get();
  EXPECT_EQ(item_2_full_path, item_2->file_path());
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *holding_space_util::ResolveImage(
           holding_space_service->thumbnail_loader_for_testing(),
           HoldingSpaceItem::Type::kScreenshot, item_2_full_path)
           ->image_skia()
           .bitmap(),
      *item_2->image().image_skia().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(
      item_2_virtual_path,
      GetVirtualPathFromUrl(item_2->file_system_url(), downloads_mount.name()));
  EXPECT_EQ(base::ASCIIToUTF16("Screenshot 2.png"), item_2->text());
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
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  HoldingSpaceModel* const primary_holding_space_model =
      HoldingSpaceController::Get()->model();

  EXPECT_EQ(primary_holding_space_model,
            primary_holding_space_service->model_for_testing());

  base::ListValue persisted_holding_space_items;

  // Verify persistent storage is updated when adding each type of item.
  for (const HoldingSpaceItem::Type type : GetHoldingSpaceItemTypes()) {
    const base::FilePath file_path = CreateArbitraryFile(downloads_mount);
    const GURL file_system_url = GetFileSystemUrl(GetProfile(), file_path);

    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        type, file_path, file_system_url,
        holding_space_util::ResolveImage(
            primary_holding_space_service->thumbnail_loader_for_testing(), type,
            file_path));

    // We do not persist `kDownload` type items.
    if (type != HoldingSpaceItem::Type::kDownload)
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

    // We do not persist `kDownload` type items.
    if (holding_space_item->type() != HoldingSpaceItem::Type::kDownload)
      persisted_holding_space_items.Remove(0, /*out_value=*/nullptr);

    primary_holding_space_model->RemoveItem(holding_space_item->id());

    EXPECT_EQ(*GetProfile()->GetPrefs()->GetList(
                  HoldingSpacePersistenceDelegate::kPersistencePath),
              persisted_holding_space_items);
  }
}

// Verifies that the holding space model is restored from persistence. Note that
// when restoring from persistence, existence of backing files is verified and
// any stale holding space items are removed.
TEST_F(HoldingSpaceKeyedServiceTest, RestorePersistentStorage) {
  // Create file system mount point.
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

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
          // We do not persist `kDownload` type items.
          if (type == HoldingSpaceItem::Type::kDownload)
            continue;

          const base::FilePath file = CreateArbitraryFile(downloads_mount);
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  holding_space_util::ResolveImage(
                      primary_holding_space_service
                          ->thumbnail_loader_for_testing(),
                      type, file));

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

          auto stale_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  base::FilePath(base::UnguessableToken::Create().ToString()),
                  GURL(),
                  std::make_unique<HoldingSpaceImage>(
                      /*placeholder=*/gfx::ImageSkia(),
                      /*async_bitmap_resolver=*/base::DoNothing()));

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

  EXPECT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  EXPECT_EQ(secondary_holding_space_model->items().size(),
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

// Verifies that screenshots restored from persistence are not older than
// kMaxFileAge.
TEST_F(HoldingSpaceKeyedServiceTest, RemoveOlderFilesFromPersistance) {
  // Create file system mount point.
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

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
          // We do not persist `kDownload` type items.
          if (type == HoldingSpaceItem::Type::kDownload)
            continue;

          const base::FilePath file = CreateArbitraryFile(downloads_mount);
          const GURL file_system_url = GetFileSystemUrl(GetProfile(), file);

          auto fresh_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type, file, file_system_url,
                  holding_space_util::ResolveImage(
                      primary_holding_space_service
                          ->thumbnail_loader_for_testing(),
                      type, file));

          persisted_holding_space_items_before_restoration->Append(
              fresh_holding_space_item->Serialize());

          // We don't expect the screenshots to still be in persistence or
          // restoration after model restoration since the file will be older
          // than the time limit for restored files.
          if (type != HoldingSpaceItem::Type::kScreenshot) {
            persisted_holding_space_items_after_restoration.Append(
                fresh_holding_space_item->Serialize());
            restored_holding_space_items.push_back(
                std::move(fresh_holding_space_item));
          }

          auto stale_holding_space_item =
              HoldingSpaceItem::CreateFileBackedItem(
                  type,
                  base::FilePath(base::UnguessableToken::Create().ToString()),
                  GURL(),
                  std::make_unique<HoldingSpaceImage>(
                      /*placeholder=*/gfx::ImageSkia(),
                      /*async_bitmap_resolver=*/base::DoNothing()));

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

  EXPECT_EQ(secondary_holding_space_model,
            secondary_holding_space_service->model_for_testing());

  EXPECT_EQ(secondary_holding_space_model->items().size(),
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

TEST_F(HoldingSpaceKeyedServiceTest, RetrieveHistory) {
  // Create a test downloads mount point.
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

  std::vector<base::FilePath> virtual_paths;
  std::vector<base::FilePath> full_paths;
  content::DownloadManager::DownloadVector download_items_mock;

  content::MockDownloadManager* mock_download_manager = download_manager();

  base::Time initial_testing_time = base::Time::Now();

  for (int i = 0; i < 3; ++i) {
    const base::FilePath download_item_virtual_path(
        "Download " + base::NumberToString(i) + ".png");
    virtual_paths.push_back(download_item_virtual_path);
    const base::FilePath download_item_full_path =
        CreateFile(downloads_mount, download_item_virtual_path,
                   "download " + base::NumberToString(i));
    full_paths.push_back(download_item_full_path);
    std::unique_ptr<download::MockDownloadItem> item(
        CreateMockDownloadItem(download_item_full_path));
    // Set one item as an download in progress, which will complete afterwards.
    if (i == 2) {
      {
        testing::InSequence s1;
        EXPECT_CALL(*item, GetState())
            .WillOnce(testing::Return(download::DownloadItem::IN_PROGRESS));
        EXPECT_CALL(*item, GetState())
            .WillOnce(testing::Return(download::DownloadItem::COMPLETE));
      }
    } else {
      EXPECT_CALL(*item, GetState())
          .WillOnce(testing::Return(download::DownloadItem::COMPLETE));
      EXPECT_CALL(*item, GetEndTime())
          .WillOnce(testing::Return(initial_testing_time +
                                    base::TimeDelta::FromHours(1)));
    }
    download_items_mock.push_back(item.release());
  }
  EXPECT_CALL(*mock_download_manager, GetAllDownloads(testing::_))
      .WillOnce(testing::SetArgPointee<0>(download_items_mock));

  holding_space_util::SetNowForTesting(initial_testing_time);

  TestingProfile* secondary_profile = CreateSecondaryProfile();
  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(2u, model->items().size());

  for (int i = 0; i < static_cast<int>(model->items().size()); ++i) {
    EXPECT_EQ(full_paths[i], model->items()[i]->file_path());
    EXPECT_EQ(virtual_paths[i],
              GetVirtualPathFromUrl(model->items()[i]->file_system_url(),
                                    downloads_mount.name()));
  }

  // Notify the holding space service of download completion. It should add
  // the object to the model.
  download::MockDownloadItem* in_progress_item =
      static_cast<download::MockDownloadItem*>(download_items_mock[2]);
  in_progress_item->NotifyObserversDownloadUpdated();

  ASSERT_EQ(3u, model->items().size());
  EXPECT_EQ(full_paths[2], model->items()[2]->file_path());
  EXPECT_EQ(virtual_paths[2],
            GetVirtualPathFromUrl(model->items()[2]->file_system_url(),
                                  downloads_mount.name()));

  for (int i = 0; i < 3; i++)
    delete download_items_mock[i];
}

TEST_F(HoldingSpaceKeyedServiceTest, AddDownloadItem) {
  TestingProfile* profile = GetProfile();
  // Create a test downloads mount point.
  ScopedDownloadsMountPoint downloads_mount(profile);
  ASSERT_TRUE(downloads_mount.IsValid());

  // Create a fake download file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath download_item_virtual_path("Download 1.png");
  const base::FilePath download_item_full_path =
      CreateFile(downloads_mount, download_item_virtual_path, "download 1");

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
                                  downloads_mount.name()));
}

TEST_F(HoldingSpaceKeyedServiceTest, RemoveOlderDownloads) {
  // Create a test downloads mount point.
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

  content::DownloadManager::DownloadVector download_items_mock;
  base::Time initial_testing_time = base::Time::Now();

  content::MockDownloadManager* mock_download_manager = download_manager();

  const base::FilePath download_item_virtual_path("Download.png");
  const base::FilePath download_item_full_path =
      CreateFile(downloads_mount, download_item_virtual_path, "download ");
  std::unique_ptr<download::MockDownloadItem> item(
      CreateMockDownloadItem(download_item_full_path));
  EXPECT_CALL(*item, GetState())
      .WillOnce(testing::Return(download::DownloadItem::COMPLETE));

  // Set an end time one hour from before kMaxFileAge is met, so it is
  // considered an older download.
  EXPECT_CALL(*item, GetEndTime())
      .WillOnce(testing::Return(initial_testing_time - kMaxFileAge -
                                base::TimeDelta::FromHours(1)));
  download_items_mock.push_back(item.get());
  EXPECT_CALL(*mock_download_manager, GetAllDownloads(testing::_))
      .WillOnce(testing::SetArgPointee<0>(download_items_mock));

  // Set holding space first enabled time to 1 day before kMaxFileAge is met
  // from now, so we can make sure downloads are being excluded due to file age
  // limit, and not due to the holding space first enabled time.
  TestingProfile* const secondary_profile = CreateSecondaryProfile(
      base::BindLambdaForTesting([&](TestingPrefStore* pref_store) {
        base::Time holding_space_start_time =
            initial_testing_time - kMaxFileAge - base::TimeDelta::FromDays(1);
        auto time_value = std::make_unique<base::Value>(base::NumberToString(
            holding_space_start_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds()));
        pref_store->SetValueSilently(
            "ash.holding_space.time_of_first_availability",
            std::move(time_value),
            PersistentPrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }));
  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());
}

TEST_F(HoldingSpaceKeyedServiceTest,
       RemoveDownloadsBeforeHoldingSpaceFirstEnabled) {
  // Create a test downloads mount point.
  ScopedDownloadsMountPoint downloads_mount(GetProfile());
  ASSERT_TRUE(downloads_mount.IsValid());

  const base::FilePath download_item_virtual_path("Download.png");
  const base::FilePath download_item_full_path =
      CreateFile(downloads_mount, download_item_virtual_path, "download ");
  std::unique_ptr<download::MockDownloadItem> item(
      CreateMockDownloadItem(download_item_full_path));
  EXPECT_CALL(*item, GetState())
      .WillOnce(testing::Return(download::DownloadItem::COMPLETE));

  // Create a file with a download time set to one hour before current time. The
  // download will be older than holding space.
  EXPECT_CALL(*item, GetEndTime())
      .WillOnce(
          testing::Return(base::Time::Now() - base::TimeDelta::FromHours(1)));

  content::DownloadManager::DownloadVector download_items_mock;
  download_items_mock.push_back(item.get());

  content::MockDownloadManager* mock_download_manager = download_manager();

  EXPECT_CALL(*mock_download_manager, GetAllDownloads(testing::_))
      .WillOnce(testing::SetArgPointee<0>(download_items_mock));

  TestingProfile* const secondary_profile = CreateSecondaryProfile();
  ActivateSecondaryProfile();
  HoldingSpaceModelAttachedWaiter(secondary_profile).Wait();

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(0u, model->items().size());
}

}  // namespace ash
