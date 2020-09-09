// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
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

using HoldingSpaceClientImplTest = HoldingSpaceBrowserTestBase;

// Verifies that `HoldingSpaceClient::OpenItem()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenItem) {
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

    // We expect `HoldingSpaceClient::OpenItem()` to fail when the backing file
    // for `holding_space_item` does not exist.
    base::RunLoop run_loop;
    holding_space_client->OpenItem(
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

    // We expect `HoldingSpaceClient::OpenItem()` to succeed when the backing
    // file for `holding_space_item` exists.
    base::RunLoop run_loop;
    holding_space_client->OpenItem(
        *holding_space_item,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Verifies that `HoldingSpaceClient::OpenItemInFolder()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenItemInFolder) {
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

    // We expect `HoldingSpaceClient::OpenItemInFolder()` to fail when the
    // backing file for `holding_space_item` does not exist.
    base::RunLoop run_loop;
    holding_space_client->OpenItemInFolder(
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

    // We expect `HoldingSpaceClient::OpenItemInFolder()` to succeed when the
    // backing file for `holding_space_item` exists.
    base::RunLoop run_loop;
    holding_space_client->OpenItemInFolder(
        *holding_space_item,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace ash
