// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu_win.h"

#include <shlobj.h>
#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"

namespace web_app {

namespace {

constexpr int kMaxJumpListItems = 10;

constexpr const char* kShortcutsMenuRegistrationHistogram =
    "WebApp.ShortcutsMenu.Win.Results";

// This should be kept in sync with ShortcutsMenuWinRegistrationResult inside
// tools/metrics/histograms/enums.xml
enum class ShortcutsMenuRegistrationResult {
  kSuccess = 0,
  kFailedToCreateShortcutMenuIconsDirectory = 1,
  kFailedToCreateIconFromImageFamily = 2,
  kFailedToBeginJumplistUpdate = 3,
  kFailedToAddLinkItemsToJumplist = 4,
  kFailedToCommitJumplistUpdate = 5,
  kMaxValue = kFailedToCommitJumplistUpdate
};

// Records the result of registering shortcuts menu on Win to UMA.
void RecordShortcutsMenuResult(ShortcutsMenuRegistrationResult result) {
  base::UmaHistogramEnumeration(kShortcutsMenuRegistrationHistogram, result);
}

// Testing hook for shell_integration_linux
UpdateJumpListForTesting& GetUpdateJumpListForTesting() {
  static base::NoDestructor<UpdateJumpListForTesting> instance;
  return *instance;
}

base::FilePath GetShortcutsMenuIconsDirectory(
    const base::FilePath& shortcut_data_dir) {
  static constexpr base::FilePath::CharType kShortcutsMenuIconsDirectoryName[] =
      FILE_PATH_LITERAL("Shortcuts Menu Icons");
  return shortcut_data_dir.Append(kShortcutsMenuIconsDirectoryName);
}

base::FilePath GetShortcutIconPath(const base::FilePath& shortcut_data_dir,
                                   int icon_index) {
  return GetShortcutsMenuIconsDirectory(shortcut_data_dir)
      .AppendASCII(base::NumberToString(icon_index) + ".ico");
}

// Writes .ico file to disk.
bool WriteShortcutsMenuIconsToIcoFiles(
    const base::FilePath& shortcut_data_dir,
    const ShortcutsMenuIconBitmaps& shortcut_icons) {
  if (!base::CreateDirectory(
          GetShortcutsMenuIconsDirectory(shortcut_data_dir))) {
    RecordShortcutsMenuResult(ShortcutsMenuRegistrationResult::
                                  kFailedToCreateShortcutMenuIconsDirectory);
    return false;
  }
  int icon_index = -1;
  for (const IconBitmaps& icon_bitmaps : shortcut_icons) {
    ++icon_index;
    if (icon_bitmaps.any.empty())
      continue;

    base::FilePath icon_file =
        GetShortcutIconPath(shortcut_data_dir, icon_index);
    gfx::ImageFamily image_family;
    for (const auto& item : icon_bitmaps.any) {
      DCHECK_NE(item.second.colorType(), kUnknown_SkColorType);
      image_family.Add(gfx::Image::CreateFrom1xBitmap(item.second));
    }
    if (!IconUtil::CreateIconFileFromImageFamily(image_family, icon_file)) {
      RecordShortcutsMenuResult(
          ShortcutsMenuRegistrationResult::kFailedToCreateIconFromImageFamily);
      return false;
    }
  }
  return true;
}

bool UpdateJumpList(
    std::wstring app_user_model_id,
    const std::vector<scoped_refptr<ShellLinkItem>>& link_items) {
  if (GetUpdateJumpListForTesting())
    return std::move(GetUpdateJumpListForTesting())
        .Run(app_user_model_id, link_items);

  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    test_override->AddShortcutsMenuJumpListEntryForApp(app_user_model_id,
                                                       link_items);
  }

  JumpListUpdater jumplist_updater(app_user_model_id);
  if (!jumplist_updater.BeginUpdate()) {
    RecordShortcutsMenuResult(
        ShortcutsMenuRegistrationResult::kFailedToBeginJumplistUpdate);
    return false;
  }

  if (!jumplist_updater.AddTasks(link_items)) {
    RecordShortcutsMenuResult(
        ShortcutsMenuRegistrationResult::kFailedToAddLinkItemsToJumplist);
    return false;
  }

  bool success = jumplist_updater.CommitUpdate();
  if (!success) {
    RecordShortcutsMenuResult(
        ShortcutsMenuRegistrationResult::kFailedToCommitJumplistUpdate);
  }
  return success;
}

}  // namespace

void SetUpdateJumpListForTesting(
    UpdateJumpListForTesting updateJumpListForTesting) {
  GetUpdateJumpListForTesting() = std::move(updateJumpListForTesting);
}

std::wstring GenerateAppUserModelId(const base::FilePath& profile_path,
                                    const webapps::AppId& app_id) {
  std::wstring app_name =
      base::UTF8ToWide(GenerateApplicationNameFromAppId(app_id));
  return shell_integration::win::GetAppUserModelIdForApp(app_name,
                                                         profile_path);
}

bool ShouldRegisterShortcutsMenuWithOs() {
  return true;
}

bool RegisterShortcutsMenuWithOsTask(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();

  // Each entry in the ShortcutsMenu (JumpList on Windows) needs an icon in .ico
  // format. This helper writes these icon files to disk as a series of
  // <index>.ico files, where index is a particular shortcut's index in the
  // shortcuts vector.
  if (!WriteShortcutsMenuIconsToIcoFiles(shortcut_data_dir,
                                         shortcuts_menu_icon_bitmaps)) {
    return false;
  }

  ShellLinkItemList shortcut_list;

  // Limit JumpList entries.
  int num_entries = std::min(static_cast<int>(shortcuts_menu_item_infos.size()),
                             kMaxJumpListItems);
  for (int i = 0; i < num_entries; i++) {
    scoped_refptr<ShellLinkItem> shortcut_link =
        base::MakeRefCounted<ShellLinkItem>();

    shortcut_link->GetCommandLine()->CopySwitchesFrom(
        *base::CommandLine::ForCurrentProcess(), {{switches::kUserDataDir}});

    // Set switches to launch shortcut items in the specified app.
    shortcut_link->GetCommandLine()->AppendSwitchASCII(switches::kAppId,
                                                       app_id);

    shortcut_link->GetCommandLine()->AppendSwitchASCII(
        switches::kAppLaunchUrlForShortcutsMenuItem,
        shortcuts_menu_item_infos[i].url.spec());

    // Set JumpList Item title and icon. The icon needs to be a .ico file.
    // We downloaded these in a shortcut icons folder in the OS integration
    // resources directory for this app.
    shortcut_link->set_title(shortcuts_menu_item_infos[i].name);
    base::FilePath shortcut_icon_path =
        GetShortcutIconPath(shortcut_data_dir, i);
    shortcut_link->set_icon(shortcut_icon_path, 0);
    shortcut_list.push_back(std::move(shortcut_link));
  }

  return UpdateJumpList(GenerateAppUserModelId(profile_path, app_id),
                        shortcut_list);
}

void OnShortcutsMenuRegistrationComplete(RegisterShortcutsMenuCallback callback,
                                         bool registration_successful) {
  if (registration_successful) {
    RecordShortcutsMenuResult(ShortcutsMenuRegistrationResult::kSuccess);
  }
  std::move(callback).Run(registration_successful ? Result::kOk
                                                  : Result::kError);
}

void RegisterShortcutsMenuWithOs(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    RegisterShortcutsMenuCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&RegisterShortcutsMenuWithOsTask, app_id, profile_path,
                     shortcut_data_dir, shortcuts_menu_item_infos,
                     shortcuts_menu_icon_bitmaps),
      base::BindOnce(&OnShortcutsMenuRegistrationComplete,
                     std::move(callback)));
}

bool UnregisterShortcutsMenuWithOs(const webapps::AppId& app_id,
                                   const base::FilePath& profile_path,
                                   RegisterShortcutsMenuCallback callback) {
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  if (test_override) {
    CHECK_IS_TEST();
    test_override->DeleteShortcutsMenuJumpListEntryForApp(
        GenerateAppUserModelId(profile_path, app_id));
  }

  bool unregistration_successful = false;
  if (!JumpListUpdater::IsEnabled()) {
    unregistration_successful = true;
  } else {
    unregistration_successful = JumpListUpdater::DeleteJumpList(
        GenerateAppUserModelId(profile_path, app_id));
  }

  std::move(callback).Run(unregistration_successful ? Result::kOk
                                                    : Result::kError);
  return unregistration_successful;
}

namespace internals {

bool DeleteShortcutsMenuIcons(const base::FilePath& shortcut_data_dir) {
  base::FilePath shortcuts_menu_icons_path =
      GetShortcutsMenuIconsDirectory(shortcut_data_dir);
  return base::DeletePathRecursively(shortcuts_menu_icons_path);
}

}  // namespace internals

}  // namespace web_app
