// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_shortcut_win.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/win/icon_util.h"

namespace omnibox_everywhere {

namespace {

constexpr wchar_t kAppName[] = L"app_search_in_chrome";

base::FilePath GetChromeProxyPath() {
  base::FilePath chrome_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_dir)) {
    return base::FilePath();
  }
  return chrome_dir.Append(FILE_PATH_LITERAL("chrome_proxy.exe"));
}

std::wstring GetDisplayName() {
  return base::UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SETTINGS_OMNIBOX_EVERYWHERE_TITLE));
}

std::wstring GetShortcutName() {
  return base::StrCat({GetDisplayName(), L".lnk"});
}

}  // namespace

std::wstring GetAppUserModelId() {
  return shell_integration::win::GetAppUserModelIdForApp(
      kAppName,
      /*profile_path=*/base::FilePath());
}

base::FilePath GetIconFilePath() {
  base::FilePath chrome_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &chrome_dir)) {
    return base::FilePath();
  }
  return chrome_dir.Append(FILE_PATH_LITERAL("SearchInChrome.ico"));
}

void SetWindowProperties(HWND hwnd, bool is_ephemeral) {
  if (!hwnd) {
    return;
  }
  if (is_ephemeral) {
    // Ephemeral widgets are hidden from the taskbar and should not be pinned.
    ui::win::PreventWindowFromPinning(hwnd);
    return;
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
      app_id, /*app_icon_path=*/GetIconFilePath(),
      /*app_icon_index=*/0, relaunch_command.GetCommandLineString(),
      /*relaunch_display_name=*/GetDisplayName(), hwnd);
}

OmniboxEverywhereShortcutHelperWin::OmniboxEverywhereShortcutHelperWin() =
    default;

OmniboxEverywhereShortcutHelperWin::~OmniboxEverywhereShortcutHelperWin() =
    default;

bool OmniboxEverywhereShortcutHelperWin::EnsureIconPersisted() {
  base::FilePath icon_path = GetIconFilePath();
  if (icon_path.empty()) {
    return false;
  }
  std::optional<int64_t> file_size = base::GetFileSize(icon_path);
  if (file_size.has_value() && *file_size > 0) {
    return true;
  }
  if (!base::CreateDirectory(icon_path.DirName())) {
    return false;
  }

  const gfx::VectorIcon& icon_vector =
      OmniboxEverywhereWidgetDelegate::GetVectorIcon();

  gfx::ImageFamily image_family;
  for (int dimension : IconUtil::kIconDimensions) {
    gfx::Canvas canvas(gfx::Size(dimension, dimension), 1.0f,
                       /*is_opaque=*/false);
    // TODO(b/532193825): Think about contrast issues with taskbar in
    // non-branded builds.
    gfx::PaintVectorIcon(&canvas, icon_vector, dimension, SK_ColorBLACK);
    image_family.Add(gfx::Image::CreateFrom1xBitmap(canvas.GetBitmap()));
  }

  if (image_family.empty()) {
    return false;
  }

  return IconUtil::CreateIconFileFromImageFamily(image_family, icon_path);
}

bool OmniboxEverywhereShortcutHelperWin::CreateStartMenuShortcut() {
  EnsureIconPersisted();

  base::FilePath start_menu_dir;
  if (!base::PathService::Get(base::DIR_START_MENU, &start_menu_dir) ||
      start_menu_dir.empty()) {
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
  shortcut_properties.set_icon(GetIconFilePath(), /*icon_index_in=*/0);
  shortcut_properties.set_description(GetDisplayName());

  base::FilePath shortcut_path = start_menu_dir.Append(GetShortcutName());

  return base::win::CreateOrUpdateShortcutLink(
      shortcut_path, shortcut_properties,
      base::win::ShortcutOperation::kCreateAlways);
}

}  // namespace omnibox_everywhere
