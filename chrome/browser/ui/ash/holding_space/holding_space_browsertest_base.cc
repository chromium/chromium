// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Creates a .txt file at the root of the downloads mount point and returns the
// path of the created file.
base::FilePath CreateTextFile(Profile* profile) {
  return test::CreateFile(profile);
}

// Creates a .png file at the root of the downloads mount point and returns the
// path of the created file.
base::FilePath CreateImageFile(Profile* profile) {
  return test::CreateFile(profile, /*extension=*/"png");
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
  SystemWebAppBrowserTestBase::SetUpOnMainThread();
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
  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile, file_path);
  const HoldingSpaceFile::FileSystemType file_system_type =
      holding_space_util::ResolveFileSystemType(profile, file_system_url);

  auto item = HoldingSpaceItem::CreateFileBackedItem(
      type, HoldingSpaceFile(file_path, file_system_type, file_system_url),
      progress,
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
    const std::optional<std::string>& extension) {
  return test::CreateFile(GetProfile(), extension.value_or("txt"));
}

void HoldingSpaceBrowserTestBase::RequestAndAwaitLockScreen() {
  if (session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  SessionManagerClient::Get()->RequestLockScreen();
  SessionStateWaiter(session_manager::SessionState::LOCKED).Wait();
}

// HoldingSpaceUiBrowserTestBase -----------------------------------------------

void HoldingSpaceUiBrowserTestBase::SetUpOnMainThread() {
  HoldingSpaceBrowserTestBase::SetUpOnMainThread();

  {
    ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    // The holding space tray will not show until the user has added a file to
    // holding space. Holding space UI browser tests don't need to assert that
    // behavior since it is already asserted in ash_unittests. As a convenience,
    // add and remove a holding space item so that the holding space tray will
    // already be showing during test execution.
    ASSERT_FALSE(test_api().IsShowingInShelf());
    RemoveItem(AddDownloadFile());
  }

  // Confirm that the holding space tray is showing in the shelf.
  ASSERT_TRUE(test_api().IsShowingInShelf());

  // Confirm that holding space model has been emptied for test execution.
  ASSERT_TRUE(HoldingSpaceController::Get()->model()->items().empty());
}

}  // namespace ash
