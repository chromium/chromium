// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/reset_shortcuts_component.h"

#include <stdint.h>
#include <windows.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/shortcut.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/parsers/parser_utils/command_line_arguments_sanitizer.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"
#include "chrome/common/chrome_switches.h"
#include "components/chrome_cleaner/public/constants/constants.h"

using base::WaitableEvent;

namespace chrome_cleaner {
namespace {
std::vector<base::FilePath> GetPathsToExplore() {
  std::vector<int> keys_of_paths_to_explore = {
      base::DIR_USER_DESKTOP,      base::DIR_COMMON_DESKTOP,
      base::DIR_USER_QUICK_LAUNCH, base::DIR_START_MENU,
      base::DIR_COMMON_START_MENU, base::DIR_TASKBAR_PINS};

  std::vector<base::FilePath> paths_to_explore;
  for (int path_key : keys_of_paths_to_explore) {
    base::FilePath path;
    if (base::PathService::Get(path_key, &path))
      paths_to_explore.push_back(path);
  }
  return paths_to_explore;
}

void ResetShortcuts(std::vector<ShortcutInformation> shortcuts,
                    const FilePathSet& chrome_exe_paths) {
  LOG(INFO) << "Number of shortcuts to reset: " << shortcuts.size();
  if (chrome_exe_paths.empty()) {
    LOG(ERROR)
        << "No known paths to Chrome installations. Cannot reset shortcuts.";
    return;
  }
  const base::FilePath& target_chrome_exe =
      *chrome_exe_paths.file_paths().begin();
  if (chrome_exe_paths.size() > 1) {
    LOG(WARNING) << "More than one chrome.exe candidate for target_path. Path "
                    "to be used: "
                 << SanitizePath(target_chrome_exe);
  }

  const base::FilePath& chrome_exe_working_dir = target_chrome_exe.DirName();

  for (const ShortcutInformation& shortcut : shortcuts) {
    base::FilePath shortcut_path(shortcut.lnk_path);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    // Clear arguments that are custom-made.
    base::win::ShortcutProperties updated_properties;
    // Use the first chrome.exe path in the set.
    updated_properties.set_target(target_chrome_exe);
    updated_properties.set_working_dir(chrome_exe_working_dir);
    // Additional Chrome profiles may have custom icons so the icon location
    // should be preserved.
    base::FilePath icon_location(shortcut.icon_location);
    updated_properties.set_icon(icon_location, shortcut.icon_index);
    base::CommandLine current_args(
        base::CommandLine::FromString(base::StringPrintf(
            L"unused_program %ls", shortcut.command_line_arguments.c_str())));
    const char* const kept_switches[] = {
        switches::kApp,
        switches::kAppId,
        switches::kProfileDirectory,
    };
    base::CommandLine desired_args(base::CommandLine::NO_PROGRAM);
    desired_args.CopySwitchesFrom(current_args, kept_switches,
                                  std::size(kept_switches));
    updated_properties.set_arguments(desired_args.GetArgumentsString());
    bool success = base::win::CreateOrUpdateShortcutLink(
        shortcut_path, updated_properties,
        base::win::ShortcutOperation::kCreateAlways);
    if (!success)
      LOG(ERROR) << "Reset shortcut failed on: "
                 << SanitizePath(shortcut.lnk_path);
  }
}
}  // namespace

ResetShortcutsComponent::ResetShortcutsComponent(
    ShortcutParserAPI* shortcut_parser)
    : shortcut_parser_(shortcut_parser) {
  shortcut_paths_to_explore_ = GetPathsToExplore();

  std::set<base::FilePath> chrome_exe_directories;
  ListChromeExeDirectories(&chrome_exe_directories);
  for (const auto& path : chrome_exe_directories) {
    chrome_exe_file_path_set_.Insert(path.Append(L"chrome.exe"));
  }
}

ResetShortcutsComponent::~ResetShortcutsComponent() {}

void ResetShortcutsComponent::PreScan() {}

void ResetShortcutsComponent::PostScan(const std::vector<UwSId>& found_pups) {
  // If no removable UwS was found, we should reset shortcuts now since there
  // won't be a post-cleanup called.
  if (found_pups.size() == 0 ||
      !PUPData::HasFlaggedPUP(found_pups, &PUPData::HasRemovalFlag)) {
    base::ScopedAllowBaseSyncPrimitives allow_sync;
    FindAndResetShortcuts();
  }
}

void ResetShortcutsComponent::PreCleanup() {}

void ResetShortcutsComponent::PostCleanup(ResultCode result_code,
                                          RebooterAPI* rebooter) {
  // If the user cancels the cleanup, don't reset shortcutss.
  if (result_code == RESULT_CODE_CANCELED)
    return;
  {
    base::ScopedAllowBaseSyncPrimitives allow_sync;
    FindAndResetShortcuts();
  }
}

void ResetShortcutsComponent::PostValidation(ResultCode result_code) {}

void ResetShortcutsComponent::OnClose(ResultCode result_code) {}

std::vector<ShortcutInformation> ResetShortcutsComponent::GetShortcuts() {
  std::vector<ShortcutInformation> shortcuts_found;
  if (!shortcut_parser_)
    return shortcuts_found;

  base::WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                            WaitableEvent::InitialState::NOT_SIGNALED);

  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      shortcut_paths_to_explore_, chrome_exe_file_path_set_,
      base::BindOnce(
          [](base::WaitableEvent* event,
             std::vector<ShortcutInformation>* shortcuts_found,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *shortcuts_found = parsed_shortcuts;
            event->Signal();
          },
          &event, &shortcuts_found));
  event.Wait();
  return shortcuts_found;
}

void ResetShortcutsComponent::FindAndResetShortcuts() {
  // A return here means that lnk shortcut analysis is not enabled on the
  // command line.
  if (!shortcut_parser_)
    return;

  std::vector<ShortcutInformation> shortcuts_found = GetShortcuts();
  ResetShortcuts(shortcuts_found, chrome_exe_file_path_set_);
}

void ResetShortcutsComponent::SetShortcutPathsToExploreForTesting(
    const std::vector<base::FilePath>& fake_shortcut_location_paths_) {
  shortcut_paths_to_explore_ = fake_shortcut_location_paths_;
}

void ResetShortcutsComponent::SetChromeExeFilePathSetForTesting(
    const FilePathSet& fake_chrome_exe_file_path_set) {
  chrome_exe_file_path_set_ = fake_chrome_exe_file_path_set;
}

}  // namespace chrome_cleaner
