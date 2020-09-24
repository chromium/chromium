// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include <string>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// File paths for test data.
constexpr char kTestDataDir[] = "chrome/test/data/chromeos/file_manager/";
constexpr char kImageFilePath[] = "image.png";
constexpr char kTextFilePath[] = "text.txt";

// Helpers ---------------------------------------------------------------------

// Returns the file for the `relative_path` in the test data directory.
base::FilePath TestFile(const std::string& relative_path) {
  base::FilePath source_root;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root));
  return source_root.AppendASCII(kTestDataDir).AppendASCII(relative_path);
}

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceClientImplTest = HoldingSpaceBrowserTestBase;

// Verifies that `HoldingSpaceClient::CopyImageToClipboard()` works as intended
// when attempting to copy both image backed and non-image backed holding space
// items.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, CopyImageToClipboard) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  {
    // Create a holding space item backed by a non-image file.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kDownload, TestFile(kTextFilePath), GURL(),
        std::make_unique<HoldingSpaceImage>(
            /*placeholder=*/gfx::ImageSkia(),
            /*async_bitmap_resolver=*/base::DoNothing()));

    // We expect `HoldingSpaceClient::CopyImageToClipboard()` to fail when the
    // backing file for `holding_space_item` is not an image file.
    base::RunLoop run_loop;
    holding_space_client->CopyImageToClipboard(
        *holding_space_item,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  {
    // Create a holding space item backed by an image file.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kDownload, TestFile(kImageFilePath), GURL(),
        std::make_unique<HoldingSpaceImage>(
            /*placeholder=*/gfx::ImageSkia(),
            /*async_bitmap_resolver=*/base::DoNothing()));

    // We expect `HoldingSpaceClient::CopyImageToClipboard()` to succeed when
    // the backing file for `holding_space_item` is an image file.
    base::RunLoop run_loop;
    holding_space_client->CopyImageToClipboard(
        *holding_space_item,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Verifies that `HoldingSpaceClient::OpenDownloads()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenDownloads) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  // We expect `HoldingSpaceClient::OpenDownloads()` to succeed.
  base::RunLoop run_loop;
  holding_space_client->OpenDownloads(
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies that `HoldingSpaceClient::OpenItems()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenItems) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  {
    // Create a holding space item backed by a non-existing file.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kDownload, base::FilePath("foo"), GURL(),
        std::make_unique<HoldingSpaceImage>(
            /*placeholder=*/gfx::ImageSkia(),
            /*async_bitmap_resolver=*/base::DoNothing()));

    // We expect `HoldingSpaceClient::OpenItems()` to fail when the backing file
    // for `holding_space_item` does not exist.
    base::RunLoop run_loop;
    holding_space_client->OpenItems(
        {holding_space_item.get()},
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  {
    // Create a holding space item backed by a newly created txt file.
    HoldingSpaceItem* holding_space_item = AddPinnedFile();

    // We expect `HoldingSpaceClient::OpenItems()` to succeed when the backing
    // file for `holding_space_item` exists.
    base::RunLoop run_loop;
    holding_space_client->OpenItems(
        {holding_space_item},
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Verifies that `HoldingSpaceClient::ShowItemInFolder()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
// Flaky on linux-chromeos-dbg (https://crbug.com/1130958)
#ifdef NDEBUG
#define MAYBE_ShowItemInFolder ShowItemInFolder
#else
#define MAYBE_ShowItemInFolder DISABLED_ShowItemInFolder
#endif
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, MAYBE_ShowItemInFolder) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  {
    // Create a holding space item backed by a non-existing file.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kDownload, base::FilePath("foo"), GURL(),
        std::make_unique<HoldingSpaceImage>(
            /*placeholder=*/gfx::ImageSkia(),
            /*async_bitmap_resolver=*/base::DoNothing()));

    // We expect `HoldingSpaceClient::ShowItemInFolder()` to fail when the
    // backing file for `holding_space_item` does not exist.
    base::RunLoop run_loop;
    holding_space_client->ShowItemInFolder(
        *holding_space_item,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  {
    // Create a holding space item backed by a newly created txt file.
    HoldingSpaceItem* holding_space_item = AddPinnedFile();

    // We expect `HoldingSpaceClient::ShowItemInFolder()` to succeed when the
    // backing file for `holding_space_item` exists.
    base::RunLoop run_loop;
    holding_space_client->ShowItemInFolder(
        *holding_space_item,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Verifies that `HoldingSpaceClient::PinItems()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, PinItems) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  auto* holding_space_model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_client && holding_space_model);

  // Create a download holding space item.
  HoldingSpaceItem* download_item = AddDownloadFile();
  ASSERT_EQ(1u, holding_space_model->items().size());

  // Attempt to pin the download holding space item.
  holding_space_client->PinItems({download_item});
  ASSERT_EQ(2u, holding_space_model->items().size());

  // The pinned holding space item should have type `kPinnedFile` but share the
  // same text and file path as the original download holding space item.
  HoldingSpaceItem* pinned_file_item = holding_space_model->items()[1].get();
  EXPECT_EQ(pinned_file_item->type(), HoldingSpaceItem::Type::kPinnedFile);
  EXPECT_EQ(download_item->text(), pinned_file_item->text());
  EXPECT_EQ(download_item->file_path(), pinned_file_item->file_path());
}

// Verifies that `HoldingSpaceClient::UnpinItems()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, UnpinItems) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  auto* holding_space_model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_client && holding_space_model);

  // Create a pinned file holding space item.
  HoldingSpaceItem* pinned_file_item = AddPinnedFile();
  ASSERT_EQ(1u, holding_space_model->items().size());

  // Attempt to unpin the pinned file holding space item.
  holding_space_client->UnpinItems({pinned_file_item});
  ASSERT_EQ(0u, holding_space_model->items().size());
}

}  // namespace ash
