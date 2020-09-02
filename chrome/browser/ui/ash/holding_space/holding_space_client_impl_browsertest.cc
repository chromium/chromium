// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

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

}  // namespace

class HoldingSpaceClientImplTest : public InProcessBrowserTest {
 public:
  HoldingSpaceClientImplTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
  }

  HoldingSpaceClientImplTest(const HoldingSpaceClientImplTest& other) = delete;
  HoldingSpaceClientImplTest& operator=(
      const HoldingSpaceClientImplTest& other) = delete;
  ~HoldingSpaceClientImplTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that `HoldingSpaceClient::OpenItem()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenItem) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  // Create a holding space item backed by a non-existing file.
  auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, base::FilePath("foo"), GURL(),
      std::make_unique<HoldingSpaceImage>(/*placeholder=*/gfx::ImageSkia()));

  {
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

  // Create a holding space item backed by a newly created txt file.
  holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, CreateTextFile(browser()->profile()),
      GURL(),
      std::make_unique<HoldingSpaceImage>(/*placeholder=*/gfx::ImageSkia()));

  {
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

}  // namespace ash
