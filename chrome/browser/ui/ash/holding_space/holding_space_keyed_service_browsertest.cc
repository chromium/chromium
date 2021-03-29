// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

// Types of file systems backing holding space items tested by
// HoldingSpaceKeyedServiceBrowserTest. The tests are parameterized by this
// enum.
enum class FileSystemType { kDownloads, kDriveFs };

// Mocks -----------------------------------------------------------------------

// Mock observer which can be used to set expectations about model behavior.
class MockHoldingSpaceModelObserver : public HoldingSpaceModelObserver {
 public:
  MOCK_METHOD(void,
              OnHoldingSpaceItemsAdded,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemsRemoved,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemFinalized,
              (const HoldingSpaceItem* item),
              (override));
};

// Helpers ---------------------------------------------------------------------

// Returns the path of the downloads mount point for the given `profile`.
base::FilePath GetDownloadsPath(Profile* profile) {
  base::FilePath result;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(profile), &result));
  return result;
}

// Creates a txt file at the path of the downloads mount point for `profile`.
base::FilePath CreateTextFile(
    const base::FilePath& root_path,
    const base::Optional<std::string>& relative_path) {
  const base::FilePath path =
      root_path.Append(relative_path.value_or(base::StringPrintf(
          "%s.txt", base::UnguessableToken::Create().ToString().c_str())));

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::CreateDirectory(path.DirName()))
    return base::FilePath();
  if (!base::WriteFile(path, /*content=*/std::string()))
    return base::FilePath();

  return path;
}

// Waits for a holding space item matching the provided `predicate` to be added
// to the holding space model. Returns immediately if the item already exists.
void WaitForItemAddition(
    base::RepeatingCallback<bool(const HoldingSpaceItem*)> predicate) {
  auto* model = ash::HoldingSpaceController::Get()->model();
  if (std::any_of(model->items().begin(), model->items().end(),
                  [&predicate](const auto& item) {
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

// Waits for a holding space item with the provided `item_id` to be added to the
// holding space model. Returns immediately if the item already exists.
void WaitForItemAddition(const std::string& item_id) {
  WaitForItemAddition(
      base::BindLambdaForTesting([&item_id](const HoldingSpaceItem* item) {
        return item->id() == item_id;
      }));
}

// Waits for a holding space item matching the provided `predicate` to be
// removed from the holding space model. Returns immediately if the model does
// not contain such an item.
void WaitForItemRemoval(
    base::RepeatingCallback<bool(const HoldingSpaceItem*)> predicate) {
  auto* model = ash::HoldingSpaceController::Get()->model();
  if (std::none_of(model->items().begin(), model->items().end(),
                   [&predicate](const auto& item) {
                     return predicate.Run(item.get());
                   })) {
    return;
  }

  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(model);

  base::RunLoop run_loop;
  ON_CALL(mock, OnHoldingSpaceItemsRemoved)
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

// Waits for a holding space item with the provided `item_id` to be removed from
// the holding space model. Returns immediately if the model does not contain
// such an item.
void WaitForItemRemoval(const std::string& item_id) {
  WaitForItemRemoval(
      base::BindLambdaForTesting([&item_id](const HoldingSpaceItem* item) {
        return item->id() == item_id;
      }));
}

// Waits for a holding space item matching the provided `predicate` to be added
// to the holding space model and finalized. Returns immediately if the item
// already exists and is finalized.
void WaitForItemFinalization(
    base::RepeatingCallback<bool(const HoldingSpaceItem*)> predicate) {
  WaitForItemAddition(predicate);

  auto* model = ash::HoldingSpaceController::Get()->model();
  auto item_it = std::find_if(
      model->items().begin(), model->items().end(),
      [&predicate](const auto& item) { return predicate.Run(item.get()); });

  DCHECK(item_it != model->items().end());
  if (item_it->get()->IsFinalized())
    return;

  testing::NiceMock<MockHoldingSpaceModelObserver> mock;
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      observer{&mock};
  observer.Observe(model);

  base::RunLoop run_loop;
  ON_CALL(mock, OnHoldingSpaceItemFinalized)
      .WillByDefault([&](const HoldingSpaceItem* item) {
        if (item == item_it->get())
          run_loop.Quit();
      });
  ON_CALL(mock, OnHoldingSpaceItemsRemoved)
      .WillByDefault([&](const std::vector<const HoldingSpaceItem*>& items) {
        for (const HoldingSpaceItem* item : items) {
          if (item != item_it->get())
            continue;
          ADD_FAILURE() << "Item unexpectedly removed: " << item->id();
          run_loop.Quit();
          return;
        }
      });
  run_loop.Run();
}

// Waits for a holding space item with the provided `item_id` to be added to the
// holding space model and finalized. Returns immediately if the item already
// exists and is finalized.
void WaitForItemFinalization(const std::string& item_id) {
  WaitForItemFinalization(
      base::BindLambdaForTesting([&item_id](const HoldingSpaceItem* item) {
        return item->id() == item_id;
      }));
}

// Adds a holding space item backed by a txt file at `item_path`.
// Returns a pointer to the added item.
const HoldingSpaceItem* AddHoldingSpaceItem(Profile* profile,
                                            const base::FilePath& item_path) {
  EXPECT_TRUE(ash::HoldingSpaceController::Get());

  auto* holding_space_model = ash::HoldingSpaceController::Get()->model();
  EXPECT_TRUE(holding_space_model);

  std::unique_ptr<HoldingSpaceItem> item =
      HoldingSpaceItem::CreateFileBackedItem(
          HoldingSpaceItem::Type::kDownload, item_path,
          holding_space_util::ResolveFileSystemUrl(profile, item_path),
          base::BindLambdaForTesting([&](HoldingSpaceItem::Type type,
                                         const base::FilePath& file_path) {
            return std::make_unique<HoldingSpaceImage>(
                HoldingSpaceImage::GetMaxSizeForType(type), file_path,
                /*async_bitmap_resolver=*/base::DoNothing());
          }));

  const HoldingSpaceItem* item_ptr = item.get();
  holding_space_model->AddItem(std::move(item));

  return item_ptr;
}

// Removes a `holding_space_item` by running the specified `closure`.
void RemoveHoldingSpaceItemViaClosure(
    const HoldingSpaceItem* holding_space_item,
    base::OnceClosure closure) {
  EXPECT_TRUE(ash::HoldingSpaceController::Get());

  const std::string item_id = holding_space_item->id();
  std::move(closure).Run();
  WaitForItemRemoval(item_id);
}

}  // namespace

// HoldingSpaceKeyedServiceBrowserTest -----------------------------------------

class HoldingSpaceKeyedServiceBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<FileSystemType> {
 public:
  HoldingSpaceKeyedServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kTemporaryHoldingSpace);
  }

  // InProcessBrowserTest:
  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      return false;

    // Mount test volumes under user data dir to ensure it gets persisted after
    // PRE test runs.
    test_mount_point_ = user_data_dir.Append("test_mount").Append("test-user");

    return GetParam() == FileSystemType::kDriveFs
               ? drive::SetUpUserDataDirectoryForDriveFsTest()
               : InProcessBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    // File system type specific setup.
    switch (GetParam()) {
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
        service_factory_for_test_ = std::make_unique<
            drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
            &create_drive_integration_service_);
        break;
    }

    EnsurePredefinedTestFiles();
  }

  base::FilePath GetTestMountPoint() { return test_mount_point_; }

  base::FilePath GetPredefinedTestFile(size_t index) const {
    DCHECK_LT(index, predefined_test_files_.size());
    return predefined_test_files_[index];
  }

  void EnsurePredefinedTestFiles() {
    if (!predefined_test_files_.empty())
      return;
    predefined_test_files_.push_back(
        CreateTextFile(GetTestMountPoint(), "root/test_file.txt"));
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    fake_drivefs_helper_ =
        std::make_unique<drive::FakeDriveFsHelper>(profile, test_mount_point_);
    integration_service_ = new drive::DriveIntegrationService(
        profile, "", test_cache_root_.GetPath(),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
    return integration_service_;
  }

  void WaitForVolumeUnmountIfNeeded() {
    // Drive fs gets unmounted on suspend, and the fake cros disks client
    // deletes the mount point on unmount event - wait for the drive mount point
    // to get deleted from file system.
    if (GetParam() != FileSystemType::kDriveFs)
      return;

    // Clear the list of predefined test files, as they are getting deleted with
    // the mount point dir.
    predefined_test_files_.clear();

    const base::FilePath mount_path = GetTestMountPoint();
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::PathExists(mount_path))
      return;

    base::RunLoop waiter_loop;
    base::FilePathWatcher watcher;
    watcher.Watch(mount_path, base::FilePathWatcher::Type::kNonRecursive,
                  base::BindRepeating(
                      [](const base::RepeatingClosure& callback,
                         const base::FilePath& path, bool error) {
                        if (!base::PathExists(path))
                          callback.Run();
                      },
                      waiter_loop.QuitClosure()));
    waiter_loop.Run();
  }

  drive::DriveIntegrationService* integration_service() {
    return integration_service_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // List of files paths that are created by default by the test suite.
  std::vector<base::FilePath> predefined_test_files_;

  // The path under which test volume is mounted.
  base::FilePath test_mount_point_;

  // Used to override downloads mount point for downloads tests.
  std::unique_ptr<base::ScopedPathOverride> downloads_override_;

  // Used to set up drive fs for for drive tests.
  base::ScopedTempDir test_cache_root_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  drive::DriveIntegrationService* integration_service_ = nullptr;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

INSTANTIATE_TEST_SUITE_P(FileSystem,
                         HoldingSpaceKeyedServiceBrowserTest,
                         ::testing::Values(FileSystemType::kDownloads,
                                           FileSystemType::kDriveFs));

// Tests -----------------------------------------------------------------------

// Verifies that holding space items are removed when their backing files
// "disappear". Note that a "disappearance" could be due to file move or delete.
IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceBrowserTest,
                       RemovesItemsWhenBackingFileDisappears) {
  // Verify that items are removed when their backing files are deleted.
  const auto* holding_space_item_to_delete = AddHoldingSpaceItem(
      browser()->profile(), CreateTextFile(GetTestMountPoint(),
                                           /*relative_path=*/base::nullopt));

  // Verify that items are removed when their backing files are moved.
  const auto* holding_space_item_to_move = AddHoldingSpaceItem(
      browser()->profile(), CreateTextFile(GetTestMountPoint(),
                                           /*relative_path=*/base::nullopt));

  RemoveHoldingSpaceItemViaClosure(
      holding_space_item_to_delete, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(
            base::DeleteFile(holding_space_item_to_delete->file_path()));
      }));

  RemoveHoldingSpaceItemViaClosure(
      holding_space_item_to_move, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(
            base::Move(holding_space_item_to_move->file_path(),
                       GetTestMountPoint().Append(
                           base::UnguessableToken::Create().ToString())));
      }));
}

IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceBrowserTest,
                       ItemsNotRemovedDuringSuspend) {
  const auto* holding_space_item =
      AddHoldingSpaceItem(browser()->profile(), GetPredefinedTestFile(0));

  auto* holding_space_model = ash::HoldingSpaceController::Get()->model();
  EXPECT_TRUE(holding_space_model);
  ASSERT_TRUE(holding_space_model->GetItem(holding_space_item->id()));
  const std::string item_id = holding_space_item->id();

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  base::RunLoop().RunUntilIdle();

  // Holding space model gets cleared on suspend.
  EXPECT_TRUE(holding_space_model->items().empty());

  // Wait for test volume unmount to finish, if necessary for the test file
  // system - for example, the drive fs will be unmounted, and fake cros disks
  // client will delete the backing directory from files system.
  WaitForVolumeUnmountIfNeeded();

  EnsurePredefinedTestFiles();
  // Verify that holding space model gets restored on resume.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  WaitForItemFinalization(item_id);
  EXPECT_TRUE(holding_space_model->GetItem(item_id));
}

// Test that creates a holding space item during PRE_ part, and verifies that
// the item gets restored after restart.
IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceBrowserTest,
                       PRE_RestoreItemsOnRestart) {
  const auto* holding_space_item =
      AddHoldingSpaceItem(browser()->profile(), GetPredefinedTestFile(0));

  auto* holding_space_model = ash::HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_model);
  EXPECT_TRUE(holding_space_model->GetItem(holding_space_item->id()));
}

IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceBrowserTest,
                       RestoreItemsOnRestart) {
  WaitForItemFinalization(
      base::BindLambdaForTesting([this](const HoldingSpaceItem* item) {
        return item->type() == HoldingSpaceItem::Type::kDownload &&
               item->file_path() == GetPredefinedTestFile(0);
      }));
}

// Verifies that drive files pinned to holding space are pinned for offline use.
IN_PROC_BROWSER_TEST_P(HoldingSpaceKeyedServiceBrowserTest,
                       PinningDriveFilesOfflineAccess) {
  // Test only for drive file system type files.
  if (GetParam() == FileSystemType::kDownloads)
    return;

  const base::FilePath file_path =
      CreateTextFile(GetTestMountPoint(),
                     /*relative_path=*/base::nullopt);
  const GURL url =
      holding_space_util::ResolveFileSystemUrl(browser()->profile(), file_path);
  storage::FileSystemURL file_system_url =
      storage::ExternalMountPoints::GetSystemInstance()->CrackURL(url);
  ASSERT_TRUE(file_system_url.is_valid());
  ASSERT_EQ(storage::kFileSystemTypeDriveFs, file_system_url.type());

  // Add item from HoldingSpaceKeyedService to handle the pinning behaviour.
  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser()->profile());
  holding_space_service->AddPinnedFiles({file_system_url});

  base::FilePath relative_path;
  ASSERT_TRUE(integration_service()->GetRelativeDrivePath(
      file_system_url.path(), &relative_path));
  base::RunLoop loop;
  bool is_pinned = false;
  integration_service()->GetDriveFsInterface()->GetMetadata(
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

}  // namespace ash
