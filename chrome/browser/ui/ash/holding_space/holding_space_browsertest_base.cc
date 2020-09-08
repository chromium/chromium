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
#include "chrome/browser/extensions/component_loader.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Creates a file at `root_path` with the specified `extension`, returning the
// path of the created file.
base::FilePath CreateFile(const base::FilePath& root_path,
                          const std::string& extension) {
  const base::FilePath file_path = root_path.Append(base::StringPrintf(
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

// Creates a .txt file at `root_path` and returns the path of the created file.
base::FilePath CreateTextFile(const base::FilePath& root_path) {
  return CreateFile(root_path, "txt");
}

// Creates a .png file at `root_path` and returns the path of the created file.
base::FilePath CreateImageFile(const base::FilePath& root_path) {
  return CreateFile(root_path, "png");
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

void HoldingSpaceBrowserTestBase::Show() {
  test_api_->Show();
}

void HoldingSpaceBrowserTestBase::Close() {
  test_api_->Close();
}

bool HoldingSpaceBrowserTestBase::IsShowing() {
  return test_api_->IsShowing();
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddPinnedFile() {
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kPinnedFile,
      /*file_path=*/CreateTextFile(scoped_temp_dir_.GetPath()),
      /*file_system_url=*/GURL(),
      /*image=*/
      std::make_unique<HoldingSpaceImage>(
          /*placeholder=*/gfx::ImageSkia(),
          /*async_bitmap_resolver=*/base::DoNothing()));

  auto* item_ptr = item.get();
  HoldingSpaceController::Get()->model()->AddItem(std::move(item));
  return item_ptr;
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddScreenshotFile() {
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kScreenshot,
      /*file_path=*/CreateImageFile(scoped_temp_dir_.GetPath()),
      /*file_system_url=*/GURL(),
      /*image=*/
      std::make_unique<HoldingSpaceImage>(
          /*placeholder=*/gfx::ImageSkia(),
          /*async_bitmap_resolver=*/base::DoNothing()));

  auto* item_ptr = item.get();
  HoldingSpaceController::Get()->model()->AddItem(std::move(item));
  return item_ptr;
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
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  test_api_ = std::make_unique<HoldingSpaceTestApi>();
}

}  // namespace ash
