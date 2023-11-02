// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include <string>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

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

HoldingSpaceBrowserTestBase::HoldingSpaceBrowserTestBase() = default;
HoldingSpaceBrowserTestBase::~HoldingSpaceBrowserTestBase() = default;

void HoldingSpaceBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
}

void HoldingSpaceBrowserTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  test_api_ = std::make_unique<HoldingSpaceTestApi>();
}

// static
aura::Window* HoldingSpaceBrowserTestBase::GetRootWindowForNewWindows() {
  return HoldingSpaceTestApi::GetRootWindowForNewWindows();
}

Profile* HoldingSpaceBrowserTestBase::GetProfile() {
  return ProfileManager::GetActiveUserProfile();
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddDownloadFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kDownload,
                 /*file_path=*/CreateTextFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddNearbyShareFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kNearbyShare,
                 /*file_path=*/CreateImageFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddPinnedFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kPinnedFile,
                 /*file_path=*/CreateTextFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddScreenshotFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kScreenshot,
                 /*file_path=*/CreateImageFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddScreenRecordingFile() {
  return AddItem(GetProfile(), HoldingSpaceItem::Type::kScreenRecording,
                 /*file_path=*/CreateImageFile(GetProfile()));
}

HoldingSpaceItem* HoldingSpaceBrowserTestBase::AddItem(
    Profile* profile,
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path,
    const HoldingSpaceProgress& progress) {
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      type, file_path,
      holding_space_util::ResolveFileSystemUrl(profile, file_path), progress,
      base::BindLambdaForTesting(
          [&](HoldingSpaceItem::Type type, const base::FilePath& path) {
            return std::make_unique<HoldingSpaceImage>(
                holding_space_util::GetMaxImageSizeForType(type), path,
                /*async_bitmap_resolver=*/base::DoNothing());
          }));

  auto* item_ptr = item.get();

  // Add holding space items through the holding space keyed service so that the
  // time of first add will be marked in preferences. The time of first add
  // contributes to deciding when the holding space tray is visible.
  HoldingSpaceKeyedServiceFactory::GetInstance()
      ->GetService(GetProfile())
      ->AddItem(std::move(item));

  return item_ptr;
}

void HoldingSpaceBrowserTestBase::RemoveItem(const HoldingSpaceItem* item) {
  HoldingSpaceController::Get()->model()->RemoveItem(item->id());
}

base::FilePath HoldingSpaceBrowserTestBase::CreateFile(
    const absl::optional<std::string>& extension) {
  return ::ash::CreateFile(GetProfile(), extension.value_or("txt"));
}

void HoldingSpaceBrowserTestBase::RequestAndAwaitLockScreen() {
  if (session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  SessionManagerClient::Get()->RequestLockScreen();
  SessionStateWaiter(session_manager::SessionState::LOCKED).Wait();
}

}  // namespace ash
