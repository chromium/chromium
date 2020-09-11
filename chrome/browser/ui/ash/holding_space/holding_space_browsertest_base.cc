// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include <string>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "base/bind_helpers.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Adds and returns a holding space item of the specified `type` backed by the
// file at the specified `file_path`.
HoldingSpaceItem* AddItem(Profile* profile,
                          HoldingSpaceItem::Type type,
                          const base::FilePath& file_path) {
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      type, file_path,
      holding_space_util::ResolveFileSystemUrl(profile, file_path),
      /*image=*/
      std::make_unique<HoldingSpaceImage>(
          /*placeholder=*/gfx::ImageSkia(),
          /*async_bitmap_resolver=*/base::DoNothing()));

  auto* item_ptr = item.get();
  HoldingSpaceController::Get()->model()->AddItem(std::move(item));
  return item_ptr;
}

// Returns the path of the downloads mount point for the given `profile`.
base::FilePath GetDownloadsPath(Profile* profile) {
  base::FilePath result;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(profile), &result));
  return result;
}

// Creates a file at the root of the downloads mount point with the specified
// `extension`, returning the path of the created file.
base::FilePath CreateFile(Profile* profile, const std::string& extension) {
  const base::FilePath file_path =
      GetDownloadsPath(profile).Append(base::StringPrintf(
          "%s.%s", base::UnguessableToken::Create().ToString().c_str(),
          extension.c_str()));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::CreateDirectory(file_path.DirName())) {
      ADD_FAILURE() << "Failed to create parent directory.";
      return base::FilePath();
    }
    if (!base::WriteFile(file_path, /*content=*/std::string())) {
      ADD_FAILURE() << "Filed to write file contents.";
      return base::FilePath();
    }
  }

  return file_path;
}

// Creates a .txt file at the root of the downloads mount point and returns the
// path of the created file.
base::FilePath CreateTextFile(Profile* profile) {
  return CreateFile(profile, "txt");
}

// Creates a .png file at the root of the downloads mount point and returns the
// path of the created file.
base::FilePath CreateImageFile(Profile* profile) {
  return CreateFile(profile, "png");
}

}  // namespace

// HoldingSpaceBrowserTestBase -------------------------------------------------

HoldingSpaceBrowserTestBase::HoldingSpaceBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
}

HoldingSpaceBrowserTestBase::~HoldingSpaceBrowserTestBase() = default;

// static
aura::Window* HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows() {
  return HoldingSpaceTestApi::GetRootWindowForNewWindows();
}

Profile* HoldingSpaceBrowserTestBase::GetProfile() {
  return ProfileManager::GetActiveUserProfile();
}

void HoldingSpaceBrowserTestBase::Show() {
  test_api_->Show();
}

void HoldingSpaceBrowserTestBase::Close() {
  test_api_->Close();
}

bool HoldingSpaceBrowserTestBase::IsShowing() {
  return test_api_->IsShowing();
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddDownloadFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kDownload,
                 /*file_path=*/CreateTextFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddPinnedFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kPinnedFile,
                 /*file_path=*/CreateTextFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddScreenshotFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kScreenshot,
                 /*file_path=*/CreateImageFile(GetProfile()));
}

std::vector<views::View*> HoldingSpaceBrowserTestBase::GetDownloadChips() {
  return test_api_->GetDownloadChips();
}

std::vector<views::View*> HoldingSpaceBrowserTestBase::GetPinnedFileChips() {
  return test_api_->GetPinnedFileChips();
}

std::vector<views::View*> HoldingSpaceBrowserTestBase::GetScreenshotViews() {
  return test_api_->GetScreenshotViews();
}

void HoldingSpaceBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
}

void HoldingSpaceBrowserTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  test_api_ = std::make_unique<HoldingSpaceTestApi>();
}

}  // namespace ash
