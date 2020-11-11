// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcuts_menu_win.h"

#include <shlobj.h>
#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"

namespace web_app {

namespace {

constexpr int kMaxJumpListItems = 10;

// UMA metric name for shortcuts menu registration result.
constexpr const char kRegistrationResultMetric[] =
    "Apps.ShortcutsMenu.Registration.Win.Result";

// UMA metric name for shortcuts menu unregistration result.
constexpr const char kUnregistrationResultMetric[] =
    "Apps.ShortcutsMenu.Unregistration.Win.Result";

// Result of shortcuts menu registration process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RegistrationResult {
  kSuccess = 0,
  kFailedToWriteIcoFilesToDisk = 1,
  kFailedToBeginUpdate = 2,
  kFailedToAddTasks = 3,
  kFailedToCommitUpdate = 4,
  kMaxValue = kFailedToCommitUpdate
};

// Result of shortcuts menu unregistration process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnregistrationResult {
  kSuccess = 0,
  kFailedToDeleteJumpList = 1,
  kMaxValue = kFailedToDeleteJumpList
};

// Records UMA metric for the result of shortcuts menu registration.
// TODO(https://crbug.com/1076042): Add tests using HistogramTester to verify
// that histograms are recorded.
void RecordRegistration(RegistrationResult result) {
  base::UmaHistogramEnumeration(kRegistrationResultMetric, result);
}

// Records UMA metric for the result of shortcuts menu unregistration.
void RecordUnregistration(UnregistrationResult result) {
  base::UmaHistogramEnumeration(kUnregistrationResultMetric, result);
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
    const ShortcutsMenuIconsBitmaps& shortcut_icons) {
  if (!base::CreateDirectory(
          GetShortcutsMenuIconsDirectory(shortcut_data_dir))) {
    return false;
  }
  int icon_index = -1;
  for (const auto& shortcut_icon : shortcut_icons) {
    ++icon_index;
    if (shortcut_icon.empty())
      continue;

    base::FilePath icon_file =
        GetShortcutIconPath(shortcut_data_dir, icon_index);
    gfx::ImageFamily image_family;
    for (const auto& item : shortcut_icon) {
      DCHECK_NE(item.second.colorType(), kUnknown_SkColorType);
      image_family.Add(gfx::Image::CreateFrom1xBitmap(item.second));
    }
    if (!IconUtil::CreateIconFileFromImageFamily(image_family, icon_file)) {
      return false;
    }
  }
  return true;
}

base::string16 GenerateAppUserModelId(const base::FilePath& profile_path,
                                      const web_app::AppId& app_id) {
  base::string16 app_name =
      base::UTF8ToUTF16(web_app::GenerateApplicationNameFromAppId(app_id));
  return shell_integration::win::GetAppUserModelIdForApp(app_name,
                                                         profile_path);
}

}  // namespace

bool ShouldRegisterShortcutsMenuWithOs() {
  return true;
}

void RegisterShortcutsMenuWithOsTask(
    const AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebApplicationShortcutsMenuItemInfo>&
        shortcuts_menu_item_infos,
    const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps) {
  // Each entry in the ShortcutsMenu (JumpList on Windows) needs an icon in .ico
  // format. This helper writes these icon files to disk as a series of
  // <index>.ico files, where index is a particular shortcut's index in the
  // shortcuts vector.
  if (!WriteShortcutsMenuIconsToIcoFiles(shortcut_data_dir,
                                         shortcuts_menu_icons_bitmaps)) {
    RecordRegistration(RegistrationResult::kFailedToWriteIcoFilesToDisk);
    return;
  }

  base::string16 app_user_model_id =
      GenerateAppUserModelId(profile_path, app_id);
  JumpListUpdater jumplist_updater(app_user_model_id);
  if (!jumplist_updater.BeginUpdate()) {
    RecordRegistration(RegistrationResult::kFailedToBeginUpdate);
    return;
  }

  ShellLinkItemList shortcut_list;

  // Limit JumpList entries.
  int num_entries = std::min(static_cast<int>(shortcuts_menu_item_infos.size()),
                             kMaxJumpListItems);
  for (int i = 0; i < num_entries; i++) {
    scoped_refptr<ShellLinkItem> shortcut_link =
        base::MakeRefCounted<ShellLinkItem>();

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
    shortcut_link->set_icon(shortcut_icon_path.value(), 0);
    shortcut_list.push_back(std::move(shortcut_link));
  }

  if (!jumplist_updater.AddTasks(shortcut_list)) {
    RecordRegistration(RegistrationResult::kFailedToAddTasks);
    return;
  }
  if (!jumplist_updater.CommitUpdate()) {
    RecordRegistration(RegistrationResult::kFailedToCommitUpdate);
    return;
  }

  RecordRegistration(RegistrationResult::kSuccess);
}

void RegisterShortcutsMenuWithOs(
    const AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebApplicationShortcutsMenuItemInfo>&
        shortcuts_menu_item_infos,
    const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&RegisterShortcutsMenuWithOsTask, app_id, profile_path,
                     shortcut_data_dir, shortcuts_menu_item_infos,
                     shortcuts_menu_icons_bitmaps));
}

bool UnregisterShortcutsMenuWithOs(const AppId& app_id,
                                   const base::FilePath& profile_path) {
  if (!JumpListUpdater::DeleteJumpList(
          GenerateAppUserModelId(profile_path, app_id))) {
    RecordUnregistration(UnregistrationResult::kFailedToDeleteJumpList);
    return false;
  }
  RecordUnregistration(UnregistrationResult::kSuccess);
  return true;
}

namespace internals {

bool DeleteShortcutsMenuIcons(const base::FilePath& shortcut_data_dir) {
  base::FilePath shortcuts_menu_icons_path =
      GetShortcutsMenuIconsDirectory(shortcut_data_dir);
  return base::DeletePathRecursively(shortcuts_menu_icons_path);
}

}  // namespace internals

}  // namespace web_app
