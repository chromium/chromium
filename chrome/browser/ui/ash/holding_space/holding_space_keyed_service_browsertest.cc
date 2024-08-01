// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_model_observer.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {
namespace {

// Types of file systems backing holding space items tested by
// HoldingSpaceKeyedServiceBrowserTest. The tests are parameterized by this
// enum.
enum class FileSystemType { kDownloads, kDriveFs };

// Helpers ---------------------------------------------------------------------

// Converts an `absolute_file_path` to its drive path.
base::FilePath ConvertAbsoluteFilePathToDrivePath(
    Profile* profile,
    const base::FilePath& absolute_file_path) {
  base::FilePath drive_path("/");
  EXPECT_TRUE(drive::DriveIntegrationServiceFactory::FindForProfile(profile)
                  ->GetMountPointPath()
                  .AppendRelativePath(absolute_file_path, &drive_path));
  return drive_path;
}

// Creates a DriveFs file change with the specified params. A `stable_id` of `0`
// signifies absence of a known `stable_id` for backwards compatibility with
// earlier versions of the `DriveFsHost`.
drivefs::mojom::FileChangePtr CreateDriveFsChange(
    drivefs::mojom::FileChange::Type type,
    const base::FilePath& drive_path,
    int64_t stable_id = 0) {
  auto change = drivefs::mojom::FileChange::New();
  change->path = drive_path;
  change->stable_id = stable_id;
  change->type = type;
  return change;
}

// Creates a txt file at the path of the downloads mount point for `profile`.
base::FilePath CreateTextFile(const base::FilePath& root_path,
                              const std::optional<std::string>& relative_path) {
  const base::FilePath path =
      root_path.Append(relative_path.value_or(base::StringPrintf(
          "%s.txt", base::UnguessableToken::Create().ToString().c_str())));

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::CreateDirectory(path.DirName()))
    return base::FilePath();
  if (!base::WriteFile(path, /*data=*/std::string())) {
    return base::FilePath();
  }

  return path;
}

// Waits for a holding space item matching the provided `predicate` to be added
// to the holding space model. Returns immediately if the item already exists.
void WaitForItemAddition(
    base::RepeatingCallback<bool(const HoldingSpaceItem*)> predicate) {
  auto* const model = HoldingSpaceController::Get()->model();
  if (base::ranges::any_of(model->items(), [&predicate](const auto& item) {
        return predicate.Run(item.get());
      })) {
    return;
  }

  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(model);

  base::RunLoop run_loop;
  ON_CALL(mock, OnHoldingSpaceItemsAdded)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        for (const HoldingSpaceItem* item : items) {
          if (predicate.Run(item)) {
            run_loop.Quit();
            return;
          }
        }
      });
  run_loop.Run();
}

// Waits for a holding space item matching the provided `predicate` to be added
// to the holding space model and initialized. Returns immediately if the item
// already exists and is initialized.
void WaitForItemInitialization(
    base::RepeatingCallback<bool(const HoldingSpaceItem*)> predicate) {
  WaitForItemAddition(predicate);

  auto* const model = HoldingSpaceController::Get()->model();
  auto item_it = base::ranges::find_if(
      model->items(),
      [&predicate](const auto& item) { return predicate.Run(item.get()); });

  DCHECK(item_it != model->items().end());
  if (item_it->get()->IsInitialized())
    return;

  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(model);

  base::RunLoop run_loop;
  ON_CALL(mock, OnHoldingSpaceItemInitialized)
      .WillByDefault([&](const HoldingSpaceItem* item) {
        if (item == item_it->get())
          run_loop.Quit();
      });
  ON_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        for (const HoldingSpaceItem* item : items) {
          if (item != item_it->get()) {
            continue;
          }
          ADD_FAILURE() << "Item unexpectedly removed: " << item->id();
          run_loop.Quit();
          return;
        }
      });
  run_loop.Run();
}

// Waits for a holding space item with the provided `item_id` to be added to the
// holding space model and initialized. Returns immediately if the item already
// exists and is initialized.
void WaitForItemInitialization(const std::string& item_id) {
  WaitForItemInitialization(
      base::BindLambdaForTesting([&item_id](const HoldingSpaceItem* item) {
        return item->id() == item_id;
      }));
}

// Adds a holding space item backed by the file at `item_path` with optional
// `progress`. Returns a pointer to the added item.
const HoldingSpaceItem* AddHoldingSpaceItem(
    Profile* profile,
    const base::FilePath& item_path,
    const HoldingSpaceProgress& progress = HoldingSpaceProgress()) {
  EXPECT_TRUE(HoldingSpaceController::Get());

  auto* const holding_space_model = HoldingSpaceController::Get()->model();
  EXPECT_TRUE(holding_space_model);

  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile, item_path);
  const HoldingSpaceFile::FileSystemType file_system_type =
      holding_space_util::ResolveFileSystemType(profile, file_system_url);

  std::unique_ptr<HoldingSpaceItem> item =
      HoldingSpaceItem::CreateFileBackedItem(
          HoldingSpaceItem::Type::kDownload,
          HoldingSpaceFile(item_path, file_system_type, file_system_url),
          progress,
          base::BindLambdaForTesting([&](HoldingSpaceItem::Type type,
                                         const base::FilePath& file_path) {
            return std::make_unique<HoldingSpaceImage>(
                holding_space_util::GetMaxImageSizeForType(type), file_path,
                /*async_bitmap_resolver=*/base::DoNothing());
          }));

  const HoldingSpaceItem* const item_ptr = item.get();
  HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile)->AddItem(
      std::move(item));
  return item_ptr;
}

// Removes a `holding_space_item` by running the specified `closure`.
void RemoveHoldingSpaceItemViaClosure(
    const HoldingSpaceItem* holding_space_item,
    base::OnceClosure closure) {
  EXPECT_TRUE(HoldingSpaceController::Get());

  const std::string item_id = holding_space_item->id();
  std::move(closure).Run();
  WaitForItemRemovalById(item_id);
}

}  // namespace

// HoldingSpaceKeyedServiceBrowserTest -----------------------------------------

class HoldingSpaceKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  explicit HoldingSpaceKeyedServiceBrowserTest(
      FileSystemType file_system_type = FileSystemType::kDriveFs)
      : file_system_type_(file_system_type) {}

  // InProcessBrowserTest:
  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      return false;

    // Mount test volumes under user data dir to ensure it gets persisted after
    // PRE test runs.
    test_mount_point_ = user_data_dir.Append("test_mount").Append("test-user");

    return file_system_type_ == FileSystemType::kDriveFs
               ? drive::SetUpUserDataDirectoryForDriveFsTest()
               : InProcessBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    // File system type specific setup.
    switch (file_system_type_) {
      case FileSystemType::kDownloads:
        // Override the default downloads path to point to the test mount point
        // within user data dir.
        downloads_override_ = std::make_unique<base::ScopedPathOverride>(
            chrome::DIR_DEFAULT_DOWNLOADS, test_mount_point_,
            /*is_absolute*/ true,
            /*create*/ false);
        break;

      case FileSystemType::kDriveFs:
        // Set up drive integration service for test.
        ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());
        create_drive_integration_service_ = base::BindRepeating(
            &HoldingSpaceKeyedServiceBrowserTest::CreateDriveIntegrationService,
            base::Unretained(this));
        drive_integration_service_factory_for_test_ = std::make_unique<
            drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
            &create_drive_integration_service_);
        break;
    }
  }

  base::FilePath GetTestMountPoint() { return test_mount_point_; }

  // Returns the cached file path if the file specified by `index` exists in
  // `predefined_test_files_`. If the file is not found, a new file path is
  // cached and returned.
  // NOTE: `index` should not exceed than the size of `predefined_test_files_`.
  base::FilePath GetPredefinedTestFile(size_t index) {
    CHECK_LE(index, predefined_test_files_.size());
    if (index == predefined_test_files_.size()) {
      predefined_test_files_.emplace_back(
          CreateTextFile(GetTestMountPoint(),
                         base::StrCat({"root/test_file_",
                                       base::NumberToString(index), ".txt"})));
    }
    return predefined_test_files_[index];
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (ash::IsSigninBrowserContext(profile) ||
        ash::IsLockScreenAppBrowserContext(profile)) {
      return nullptr;
    }

    fake_drivefs_helper_ =
        std::make_unique<drive::FakeDriveFsHelper>(profile, test_mount_point_);
    drive_integration_service_ = new drive::DriveIntegrationService(
        profile, "", test_cache_root_.GetPath(),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
    return drive_integration_service_;
  }

  drive::DriveIntegrationService* drive_integration_service() {
    return drive_integration_service_;
  }

  mojo::Remote<drivefs::mojom::DriveFsDelegate>& drivefs_delegate() {
    return fake_drivefs_helper_->fake_drivefs().delegate();
  }

 private:
  // List of files paths that are created by default by the test suite.
  std::vector<base::FilePath> predefined_test_files_;

  // The path under which test volume is mounted.
  base::FilePath test_mount_point_;

  // Used to override downloads mount point for downloads tests.
  std::unique_ptr<base::ScopedPathOverride> downloads_override_;

  // The file system used for an individual test case.
  FileSystemType file_system_type_;

  // Used to set up drive fs for for drive tests.
  base::ScopedTempDir test_cache_root_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  raw_ptr<drive::DriveIntegrationService, DanglingUntriaged>
      drive_integration_service_ = nullptr;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      drive_integration_service_factory_for_test_;
};

// Tests -----------------------------------------------------------------------

// Verifies item addition during holding space keyed service suspension.
IN_PROC_BROWSER_TEST_F(HoldingSpaceKeyedServiceBrowserTest,
                       AddItemDuringSuspension) {
  // Add a holding space item before service suspension.
  const auto* const holding_space_item = AddHoldingSpaceItem(
      browser()->profile(), GetPredefinedTestFile(/*index=*/0));
  const auto* const holding_space_model =
      HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_model);
  EXPECT_TRUE(holding_space_model->GetItem(holding_space_item->id()));
  const std::string existing_item_id = holding_space_item->id();

  // Suspend the holding space keyed service. Wait until the existing holding
  // space item is removed.
  auto* const fake_power_manager_client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_TRUE(fake_power_manager_client);
  fake_power_manager_client->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  WaitForItemRemovalById(existing_item_id);
  EXPECT_TRUE(holding_space_model->items().empty());

  // Try to add a new holding space item.
  AddHoldingSpaceItem(browser()->profile(), GetPredefinedTestFile(/*index=*/1));

  // Verify the absence of any new items.
  EXPECT_TRUE(holding_space_model->items().empty());

  // End suspension. Wait until `holding_space_item` is restored.
  fake_power_manager_client->SendSuspendDone();
  WaitForItemInitialization(existing_item_id);
  EXPECT_TRUE(holding_space_model->GetItem(existing_item_id));

  // Add a holding space item after suspension. Verify that a new item is added.
  const HoldingSpaceItem* const item = AddHoldingSpaceItem(
      browser()->profile(), GetPredefinedTestFile(/*index=*/1));
  ASSERT_TRUE(item);
  WaitForItemInitialization(item->id());
  EXPECT_TRUE(holding_space_model->GetItem(item->id()));
}

// Verifies that holding space is updated in response to DriveFs file changes.
IN_PROC_BROWSER_TEST_F(HoldingSpaceKeyedServiceBrowserTest,
                       UpdateItemsOnDriveFsFileChange) {
  // Verify holding space service exists.
  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser()->profile());
  ASSERT_TRUE(holding_space_service);

  // Verify holding space model exists.
  const auto* holding_space_model = holding_space_service->model_for_testing();
  ASSERT_TRUE(holding_space_model);

  // Add an item to holding space.
  base::FilePath src = GetPredefinedTestFile(/*index=*/0);
  auto* item = AddHoldingSpaceItem(browser()->profile(), src);

  // Verify the item exists in the model.
  ASSERT_EQ(holding_space_model->items().size(), 1u);
  EXPECT_EQ(holding_space_model->items()[0].get(), item);
  EXPECT_EQ(item->file().file_path, src);

  base::FilePath dst = GetPredefinedTestFile(/*index=*/1);

  // Prep a batch of `changes` to indicate that `src` has moved to `dst`. Note
  // the consistent `stable_id` to link the `kDelete` with the `kCreate` change.
  std::vector<drivefs::mojom::FileChangePtr> changes;
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), src),
      /*stable_id=*/1));
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kCreate,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), dst),
      /*stable_id=*/1));

  // Simulate the `changes` being sent from the server.
  drivefs_delegate()->OnFilesChanged(std::move(changes));
  drivefs_delegate().FlushForTesting();

  // Expect the holding space item to have been updated in place to reflect
  // the new location of its backing file.
  ASSERT_EQ(holding_space_model->items().size(), 1u);
  EXPECT_EQ(holding_space_model->items()[0].get(), item);
  EXPECT_EQ(item->file().file_path, dst);

  std::swap(src, dst);

  // Prep a batch of `changes` to indicate that `src` has been deleted and that
  // `dst` has been created. Note the different `stable_id` to indicate that the
  // `kDelete` and `kCreate` changes refer to different documents.
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), src),
      /*stable_id=*/1));
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kCreate,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), dst),
      /*stable_id=*/2));

  // Simulate the `changes` being sent from the server.
  drivefs_delegate()->OnFilesChanged(std::move(changes));
  drivefs_delegate().FlushForTesting();

  // Because `src` was deleted, the holding space item should be removed.
  WaitForItemRemovalById(item->id());

  // Add another holding space item, again pointing to `src`.
  item = AddHoldingSpaceItem(browser()->profile(), src);

  // Verify the item exists in the model.
  ASSERT_EQ(holding_space_model->items().size(), 1u);
  EXPECT_EQ(holding_space_model->items()[0].get(), item);
  EXPECT_EQ(item->file().file_path, src);

  // Prep a batch of `changes` to indicate that `src` has been deleted and that
  // `dst` has been created. Note the absence of `stable_id`. The `kDelete` and
  // `kCreate` events may constitute a move of the same document, but because
  // `stable_id` is absent, we can't assume that to be the case.
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), src)));
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kCreate,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), dst)));

  // Simulate the `changes` being sent from the server.
  drivefs_delegate()->OnFilesChanged(std::move(changes));
  drivefs_delegate().FlushForTesting();

  // Because `src` was deleted and cannot be determined to refer to the same
  // document that was created at `dst`, the holding space item should be
  // removed.
  WaitForItemRemovalById(item->id());

  // Add another holding space item, again pointing to `src`.
  item = AddHoldingSpaceItem(browser()->profile(), src);

  // Verify the item exists in the model.
  ASSERT_EQ(holding_space_model->items().size(), 1u);
  EXPECT_EQ(holding_space_model->items()[0].get(), item);
  EXPECT_EQ(item->file().file_path, src);

  // Prep a batch of `changes` to indicate that `src` has moved to `dst` and has
  // then been deleted. Note the consistent `stable_id` to associate all changes
  // with the same document.
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), src),
      /*stable_id=*/1));
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kCreate,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), dst),
      /*stable_id=*/1));
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), dst),
      /*stable_id=*/1));

  // Simulate the `changes` being sent from the server.
  drivefs_delegate()->OnFilesChanged(std::move(changes));
  drivefs_delegate().FlushForTesting();

  // Because the document was ultimately deleted, the holding space item should
  // be removed.
  WaitForItemRemovalById(item->id());

  // Add another holding space item, pointing to `src` in `src_dir`.
  base::FilePath src_dir = GetTestMountPoint().Append("src/");
  src = CreateTextFile(src_dir, /*relative_path=*/std::nullopt);
  item = AddHoldingSpaceItem(browser()->profile(), src);

  // Verify the item exists in the model.
  ASSERT_EQ(holding_space_model->items().size(), 1u);
  EXPECT_EQ(holding_space_model->items()[0].get(), item);
  EXPECT_EQ(item->file().file_path, src);

  base::FilePath dst_dir = GetTestMountPoint().Append("dst/");
  dst = CreateTextFile(
      dst_dir,
      /*relative_path=*/base::UTF16ToUTF8(src.BaseName().LossyDisplayName()));

  // Prep a batch of `changes` to indicate that `src_dir` has moved to
  // `dst_dir`. Note the consistent `stable_id` to link the `kDelete` with the
  // `kCreate` change.
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), src_dir),
      /*stable_id=*/1));
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kCreate,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), dst_dir),
      /*stable_id=*/1));

  // Simulate the `changes` being sent from the server.
  drivefs_delegate()->OnFilesChanged(std::move(changes));
  drivefs_delegate().FlushForTesting();

  // Expect the holding space item to have been updated in place to reflect
  // the new location of its backing file.
  ASSERT_EQ(holding_space_model->items().size(), 1u);
  EXPECT_EQ(holding_space_model->items()[0].get(), item);
  EXPECT_EQ(item->file().file_path, dst);

  std::swap(src_dir, dst_dir);
  std::swap(src, dst);

  // Prep a batch of `changes` to indicate that `src_dir` has been deleted.
  changes.push_back(CreateDriveFsChange(
      drivefs::mojom::FileChange::Type::kDelete,
      ConvertAbsoluteFilePathToDrivePath(browser()->profile(), src_dir)));

  // Simulate the `changes` being sent from the server.
  drivefs_delegate()->OnFilesChanged(std::move(changes));
  drivefs_delegate().FlushForTesting();

  // Because the parent directory in which the holding space item's backing file
  // is contained has been deleted, the holding space item should be removed.
  WaitForItemRemovalById(item->id());
}

// Verifies that drive files pinned to holding space are pinned for offline use.
IN_PROC_BROWSER_TEST_F(HoldingSpaceKeyedServiceBrowserTest,
                       PinningDriveFilesOfflineAccess) {
  const GURL url = holding_space_util::ResolveFileSystemUrl(
      browser()->profile(), GetPredefinedTestFile(/*index=*/0));
  storage::FileSystemURL file_system_url =
      storage::ExternalMountPoints::GetSystemInstance()->CrackURL(
          url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
  ASSERT_TRUE(file_system_url.is_valid());
  ASSERT_EQ(storage::kFileSystemTypeDriveFs, file_system_url.type());

  // Add item from HoldingSpaceKeyedService to handle the pinning behaviour.
  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser()->profile());
  holding_space_service->AddPinnedFiles(
      {file_system_url}, holding_space_metrics::EventSource::kTest);

  base::FilePath relative_path;
  ASSERT_TRUE(drive_integration_service()->GetRelativeDrivePath(
      file_system_url.path(), &relative_path));
  base::RunLoop loop;
  bool is_pinned = false;
  drive_integration_service()->GetDriveFsInterface()->GetMetadata(
      relative_path,
      base::BindOnce(
          [](base::RunLoop* loop, bool* is_pinned, ::drive::FileError error,
             drivefs::mojom::FileMetadataPtr metadata) {
            *is_pinned = metadata->pinned;
            loop->Quit();
          },
          &loop, &is_pinned));
  loop.Run();
  EXPECT_TRUE(is_pinned);
}

// HoldingSpaceKeyedServiceFlexibleFsBrowserTest -------------------------------

class HoldingSpaceKeyedServiceFlexibleFsBrowserTest
    : public HoldingSpaceKeyedServiceBrowserTest,
      public ::testing::WithParamInterface<FileSystemType> {
 public:
  HoldingSpaceKeyedServiceFlexibleFsBrowserTest()
      : HoldingSpaceKeyedServiceBrowserTest(GetParam()) {}

 private:
  // HoldingSpaceKeyedServiceBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    HoldingSpaceKeyedServiceBrowserTest::SetUpInProcessBrowserTestFixture();

    // Ensure the existence of a test file to verify persistence restoration.
    GetPredefinedTestFile(/*index=*/0);
  }
};

INSTANTIATE_TEST_SUITE_P(FileSystem,
                         HoldingSpaceKeyedServiceFlexibleFsBrowserTest,
                         ::testing::Values(FileSystemType::kDownloads,
                                           FileSystemType::kDriveFs));

// Tests -----------------------------------------------------------------------

// Verifies that completed holding space items are removed when their backing
// files "disappear". Note that a "disappearance" could be due to a file move or
// delete. In-progress holding space items are not subject to the same backing
// file path validity checks and may outlive their backing files.
IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceFlexibleFsBrowserTest,
                       RemovesItemsWhenBackingFileDisappears) {
  // Create an `in_progress_holding_space_item_to_delete`.
  const auto* in_progress_holding_space_item_to_delete = AddHoldingSpaceItem(
      browser()->profile(),
      CreateTextFile(GetTestMountPoint(),
                     /*relative_path=*/std::nullopt),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100));

  {
    // Delete its backing file. Later we will confirm that the associated
    // holding space item still exists after we are sure that scheduled validity
    // checks have run.
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::DeleteFile(
        in_progress_holding_space_item_to_delete->file().file_path));
  }

  // Create a completed `holding_space_item_to_delete`.
  const auto* holding_space_item_to_delete = AddHoldingSpaceItem(
      browser()->profile(), CreateTextFile(GetTestMountPoint(),
                                           /*relative_path=*/std::nullopt));

  // Delete its backing file and verify that it is removed from holding space.
  // Note that this guarantees that scheduled validity checks will have run.
  RemoveHoldingSpaceItemViaClosure(
      holding_space_item_to_delete, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(
            base::DeleteFile(holding_space_item_to_delete->file().file_path));
      }));

  // Now that scheduled validity checks have run, verify that the in-progress
  // item whose backing file was deleted still exists in the model.
  auto* model = HoldingSpaceController::Get()->model();
  EXPECT_TRUE(model->GetItem(in_progress_holding_space_item_to_delete->id()));

  // Create an `in_progress_holding_space_item_to_move`.
  const auto* in_progress_holding_space_item_to_move = AddHoldingSpaceItem(
      browser()->profile(),
      CreateTextFile(GetTestMountPoint(),
                     /*relative_path=*/std::nullopt),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100));

  {
    // Move its backing file. Later we will confirm that the associated holding
    // space item still exists after we are sure that scheduled validity checks
    // have run.
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        base::Move(in_progress_holding_space_item_to_move->file().file_path,
                   GetTestMountPoint().Append(
                       base::UnguessableToken::Create().ToString())));
  }

  // Create a completed `holding_space_item_to_move`.
  const auto* holding_space_item_to_move = AddHoldingSpaceItem(
      browser()->profile(), CreateTextFile(GetTestMountPoint(),
                                           /*relative_path=*/std::nullopt));

  // Move its backing file and verify that it is removed from holding space.
  // Note that this guarantees that scheduled validity checks will have run.
  RemoveHoldingSpaceItemViaClosure(
      holding_space_item_to_move, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(
            base::Move(holding_space_item_to_move->file().file_path,
                       GetTestMountPoint().Append(
                           base::UnguessableToken::Create().ToString())));
      }));

  // Now that scheduled validity checks have run, verify that the in-progress
  // item whose backing file was moved still exists in the model.
  EXPECT_TRUE(model->GetItem(in_progress_holding_space_item_to_move->id()));

  // Remove all holding space items. This will clear all file system watches.
  model->RemoveAll();
  EXPECT_EQ(model->items().size(), 0u);

  // Add an `in_progress_holding_space_item_to_complete`. Because the item is
  // in-progress, no file system watch should have been registered.
  const auto* in_progress_holding_space_item_to_complete = AddHoldingSpaceItem(
      browser()->profile(),
      CreateTextFile(GetTestMountPoint(),
                     /*relative_path=*/std::nullopt),
      HoldingSpaceProgress(/*current_bytes=*/0, /*total_bytes=*/100));

  // Complete the item. This should result in a file system watch being
  // registered for the backing file's parent directory.
  model->UpdateItem(in_progress_holding_space_item_to_complete->id())
      ->SetProgress(
          HoldingSpaceProgress(/*current_bytes=*/100, /*total_bytes=*/100));

  // Delete its backing file and verify that it is removed from holding space
  // since the now completed item will be subject to validity checks.
  RemoveHoldingSpaceItemViaClosure(
      in_progress_holding_space_item_to_complete,
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(base::DeleteFile(
            in_progress_holding_space_item_to_complete->file().file_path));
      }));
}

IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceFlexibleFsBrowserTest,
                       ItemsNotRemovedDuringSuspend) {
  const auto* holding_space_item =
      AddHoldingSpaceItem(browser()->profile(), GetPredefinedTestFile(0));

  auto* holding_space_model = HoldingSpaceController::Get()->model();
  EXPECT_TRUE(holding_space_model);
  ASSERT_TRUE(holding_space_model->GetItem(holding_space_item->id()));
  const std::string item_id = holding_space_item->id();

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  base::RunLoop().RunUntilIdle();

  // Holding space model gets cleared on suspend.
  EXPECT_TRUE(holding_space_model->items().empty());

  // Verify that holding space model gets restored on resume.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  WaitForItemInitialization(item_id);
  EXPECT_TRUE(holding_space_model->GetItem(item_id));
}

// Test that creates a holding space item during PRE_ part, and verifies that
// the item gets restored after restart.
IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceFlexibleFsBrowserTest,
                       PRE_RestoreItemsOnRestart) {
  const auto* holding_space_item =
      AddHoldingSpaceItem(browser()->profile(), GetPredefinedTestFile(0));

  auto* holding_space_model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_model);
  EXPECT_TRUE(holding_space_model->GetItem(holding_space_item->id()));
}

IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceFlexibleFsBrowserTest,
                       RestoreItemsOnRestart) {
  WaitForItemInitialization(
      base::BindLambdaForTesting([this](const HoldingSpaceItem* item) {
        return item->type() == HoldingSpaceItem::Type::kDownload &&
               item->file().file_path == GetPredefinedTestFile(0);
      }));
}

}  // namespace ash
