// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

// Mocks -----------------------------------------------------------------------

// Mock observer which can be used to set expectations about model behavior.
class MockHoldingSpaceModelObserver : public HoldingSpaceModelObserver {
 public:
  MOCK_METHOD(void,
              OnHoldingSpaceItemAdded,
              (const HoldingSpaceItem* item),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemRemoved,
              (const HoldingSpaceItem* item),
              (override));
};

// Helpers ---------------------------------------------------------------------

// Posts a task and waits for it to run in order to flush the message loop.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

// Returns the path of the downloads mount point for the given `profile`.
base::FilePath GetDownloadsPath(Profile* profile) {
  base::FilePath result;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(profile), &result));
  return result;
}

// Creates a txt file at the path of the downloads mount point for `profile`.
base::FilePath CreateTextFile(Profile* profile) {
  const std::string relative_path = base::StringPrintf(
      "%s.txt", base::UnguessableToken::Create().ToString().c_str());
  const base::FilePath path = GetDownloadsPath(profile).Append(relative_path);

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::CreateDirectory(path.DirName()))
    return base::FilePath();
  if (!base::WriteFile(path, /*content=*/std::string()))
    return base::FilePath();

  return path;
}

// Adds a holding space item backed by a txt file at the path of the downloads
// mount point for `profile`. A pointer to the added item is returned.
const HoldingSpaceItem* AddHoldingSpaceItem(Profile* profile) {
  EXPECT_TRUE(ash::HoldingSpaceController::Get());

  auto* holding_space_model = ash::HoldingSpaceController::Get()->model();
  EXPECT_TRUE(holding_space_model);

  const HoldingSpaceItem* result = nullptr;

  testing::StrictMock<MockHoldingSpaceModelObserver> mock;
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> observer{&mock};
  observer.Add(holding_space_model);

  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemAdded)
      .WillOnce([&](const HoldingSpaceItem* item) {
        result = item;

        // Explicitly flush the message loop after a holding space `item` is
        // added to give file system watchers a chance to register.
        FlushMessageLoop();
        run_loop.Quit();
      });

  holding_space_model->AddItem(HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, CreateTextFile(profile), GURL(),
      std::make_unique<HoldingSpaceImage>(
          /*placeholder=*/gfx::ImageSkia(),
          /*async_bitmap_resolver=*/base::DoNothing())));

  run_loop.Run();
  return result;
}

// Removes a `holding_space_item` by running the specified `closure`.
void RemoveHoldingSpaceItemViaClosure(
    const HoldingSpaceItem* holding_space_item,
    base::OnceClosure closure) {
  EXPECT_TRUE(ash::HoldingSpaceController::Get());

  auto* holding_space_model = ash::HoldingSpaceController::Get()->model();
  EXPECT_TRUE(holding_space_model);

  testing::StrictMock<MockHoldingSpaceModelObserver> mock;
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> observer{&mock};
  observer.Add(holding_space_model);

  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnHoldingSpaceItemRemoved)
      .WillOnce([&](const HoldingSpaceItem* item) {
        EXPECT_EQ(holding_space_item, item);
        run_loop.Quit();
      });

  std::move(closure).Run();
  run_loop.Run();
}

}  // namespace

// HoldingSpaceKeyedServiceBrowserTest -----------------------------------------

class HoldingSpaceKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  HoldingSpaceKeyedServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kTemporaryHoldingSpace);
  }

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that holding space items are removed when their backing files
// "disappear". Note that a "disappearance" could be due to file move or delete.
IN_PROC_BROWSER_TEST_F(HoldingSpaceKeyedServiceBrowserTest,
                       RemovesItemsWhenBackingFileDisappears) {
  {
    // Verify that items are removed when their backing files are deleted.
    const auto* holding_space_item = AddHoldingSpaceItem(browser()->profile());
    RemoveHoldingSpaceItemViaClosure(
        holding_space_item, base::BindLambdaForTesting([&]() {
          base::ScopedAllowBlockingForTesting allow_blocking;
          EXPECT_TRUE(base::DeleteFile(holding_space_item->file_path()));
        }));
  }

  {
    // Verify that items are removed when their backing files are moved.
    const auto* holding_space_item = AddHoldingSpaceItem(browser()->profile());
    RemoveHoldingSpaceItemViaClosure(
        holding_space_item, base::BindLambdaForTesting([&]() {
          base::ScopedAllowBlockingForTesting allow_blocking;
          EXPECT_TRUE(base::Move(
              holding_space_item->file_path(),
              GetDownloadsPath(browser()->profile())
                  .Append(base::UnguessableToken::Create().ToString())));
        }));
  }
}

}  // namespace ash
