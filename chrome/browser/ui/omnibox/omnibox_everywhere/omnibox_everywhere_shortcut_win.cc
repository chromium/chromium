// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_shortcut_win.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/install_static/install_details.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/shell.h"

namespace omnibox_everywhere {

namespace {

constexpr wchar_t kAppName[] = L"app_search_with_chrome";

base::FilePath GetChromeExePath() {
  base::FilePath chrome_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_dir)) {
    return base::FilePath();
  }
  return chrome_dir.Append(chrome::kBrowserProcessExecutableName);
}

base::FilePath GetChromeProxyPath() {
  base::FilePath chrome_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_dir)) {
    return base::FilePath();
  }
  return chrome_dir.Append(FILE_PATH_LITERAL("chrome_proxy.exe"));
}

int GetDisplayNameMessageId() {
  if (install_static::InstallDetails::Get().is_primary_mode()) {
    return IDS_OMNIBOX_EVERYWHERE_NAME;
  }
  switch (chrome::GetChannel()) {
    case version_info::Channel::BETA:
      return IDS_OMNIBOX_EVERYWHERE_NAME_BETA;
    case version_info::Channel::DEV:
      return IDS_OMNIBOX_EVERYWHERE_NAME_DEV;
    case version_info::Channel::CANARY:
      return IDS_OMNIBOX_EVERYWHERE_NAME_CANARY;
    default:
      return IDS_OMNIBOX_EVERYWHERE_NAME;
  }
}

std::wstring GetShortcutName() {
  return base::StrCat({GetDisplayName(), L".lnk"});
}

// Returns an empty path if the Start Menu directory is unavailable.
base::FilePath GetStartMenuShortcutPath() {
  base::FilePath start_menu_dir;
  if (!base::PathService::Get(base::DIR_START_MENU, &start_menu_dir) ||
      start_menu_dir.empty()) {
    return base::FilePath();
  }
  return start_menu_dir.Append(GetShortcutName());
}

// Returns whether the shortcut at `shortcut_path` already resolves to the
// launch properties in `expected`.
bool ShortcutMatches(const base::FilePath& shortcut_path,
                     const base::win::ShortcutProperties& expected) {
  base::win::ShortcutProperties existing;
  if (!base::win::ResolveShortcutProperties(
          shortcut_path,
          base::win::ShortcutProperties::PROPERTIES_TARGET |
              base::win::ShortcutProperties::PROPERTIES_ARGUMENTS |
              base::win::ShortcutProperties::PROPERTIES_ICON |
              base::win::ShortcutProperties::PROPERTIES_APP_ID,
          &existing)) {
    return false;
  }
  return base::FilePath::CompareEqualIgnoreCase(existing.target.value(),
                                                expected.target.value()) &&
         existing.arguments == expected.arguments &&
         existing.app_id == expected.app_id &&
         base::FilePath::CompareEqualIgnoreCase(existing.icon.value(),
                                                expected.icon.value()) &&
         existing.icon_index == expected.icon_index;
}

}  // namespace

std::wstring GetDisplayName() {
  return base::UTF16ToWide(
      l10n_util::GetStringUTF16(GetDisplayNameMessageId()));
}

std::wstring GetAppUserModelId() {
  return shell_integration::win::GetAppUserModelIdForApp(
      kAppName,
      /*profile_path=*/base::FilePath());
}

void SetWindowProperties(HWND hwnd, bool is_ephemeral, bool allow_pinning) {
  if (!hwnd) {
    return;
  }
  if (is_ephemeral) {
    // Ephemeral widgets are hidden from the taskbar and should not be pinned.
    ui::win::PreventWindowFromPinning(hwnd);
    return;
  }

  // Must precede SetAppDetailsForWindow(): the Shell ignores PreventPinning
  // once the AUMID is set.
  if (!allow_pinning) {
    ui::win::PreventWindowFromPinning(hwnd);
  }

  // In persistent mode, assign the dedicated AppUserModelId and relaunch
  // command so the widget groups separately on the taskbar and can be pinned.
  std::wstring app_id = GetAppUserModelId();
  base::FilePath target_exe = GetChromeProxyPath();
  base::CommandLine relaunch_command(
      target_exe.empty() ? base::CommandLine::ForCurrentProcess()->GetProgram()
                         : target_exe);
  relaunch_command.AppendSwitch(switches::kOmniboxEverywhere);

  ui::win::SetAppDetailsForWindow(
      app_id, /*app_icon_path=*/GetChromeExePath(),
      /*app_icon_index=*/icon_resources::kOmniboxEverywhereIndex,
      relaunch_command.GetCommandLineString(),
      /*relaunch_display_name=*/GetDisplayName(), hwnd);
}

OmniboxEverywhereShortcutHelperWin::OmniboxEverywhereShortcutHelperWin() =
    default;

OmniboxEverywhereShortcutHelperWin::~OmniboxEverywhereShortcutHelperWin() =
    default;

bool OmniboxEverywhereShortcutHelperWin::CreateStartMenuShortcut() {
  const base::FilePath shortcut_path = GetStartMenuShortcutPath();
  if (shortcut_path.empty()) {
    return false;
  }

  base::FilePath chrome_proxy_path = GetChromeProxyPath();
  if (chrome_proxy_path.empty()) {
    return false;
  }

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(chrome_proxy_path);
  shortcut_properties.set_arguments(
      base::StrCat({L"--", base::ASCIIToWide(switches::kOmniboxEverywhere)}));
  shortcut_properties.set_app_id(GetAppUserModelId());
  shortcut_properties.set_icon(GetChromeExePath(),
                               icon_resources::kOmniboxEverywhereIndex);
  shortcut_properties.set_description(GetDisplayName());

  // A shortcut left by an older install may point at a stale target or AUMID,
  // which breaks taskbar pinning, so rewrite anything that does not match.
  if (base::PathExists(shortcut_path) &&
      ShortcutMatches(shortcut_path, shortcut_properties)) {
    return true;
  }

  return base::win::CreateOrUpdateShortcutLink(
      shortcut_path, shortcut_properties,
      base::win::ShortcutOperation::kCreateAlways);
}

}  // namespace omnibox_everywhere
