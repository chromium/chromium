// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"

namespace updater::ui {

namespace {

struct FindProcessWindowsRecord {
  uint32_t process_id = 0;
  uint32_t window_flags = 0;
  raw_ptr<std::vector<HWND>> windows = nullptr;
};

BOOL CALLBACK FindProcessWindowsEnumProc(HWND hwnd, LPARAM lparam) {
  FindProcessWindowsRecord* enum_record =
      reinterpret_cast<FindProcessWindowsRecord*>(lparam);
  CHECK(enum_record);

  DWORD process_id = 0;
  ::GetWindowThreadProcessId(hwnd, &process_id);

  if (enum_record->process_id != process_id) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustBeTopLevel) &&
      ::GetParent(hwnd)) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustHaveSysMenu) &&
      !(GetWindowLong(hwnd, GWL_STYLE) & WS_SYSMENU)) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustBeVisible) &&
      !::IsWindowVisible(hwnd)) {
    return true;
  }

  enum_record->windows->push_back(hwnd);
  return true;
}

}  // namespace

bool FindProcessWindows(uint32_t process_id,
                        uint32_t window_flags,
                        std::vector<HWND>* windows) {
  CHECK(windows);
  windows->clear();
  FindProcessWindowsRecord enum_record = {0};
  enum_record.process_id = process_id;
  enum_record.window_flags = window_flags;
  enum_record.windows = windows;
  ::EnumWindows(FindProcessWindowsEnumProc,
                reinterpret_cast<LPARAM>(&enum_record));
  const size_t num_windows = enum_record.windows->size();
  return num_windows > 0;
}

void MakeWindowForeground(HWND wnd) {
  if (!::IsWindowVisible(wnd)) {
    return;
  }
  ::SetWindowPos(wnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

bool IsMainWindow(HWND wnd) {
  return nullptr == ::GetParent(wnd) && ::IsWindowVisible(wnd);
}

bool HasSystemMenu(HWND wnd) {
  return (::GetWindowLong(wnd, GWL_STYLE) & WS_SYSMENU) != 0;
}

HRESULT SetWindowIcon(HWND hwnd, WORD icon_id, HICON* hicon) {
  CHECK(hwnd);
  CHECK(hicon);

  *hicon = nullptr;

  const int cx = ::GetSystemMetrics(SM_CXICON);
  const int cy = ::GetSystemMetrics(SM_CYICON);
  HINSTANCE exe_instance = static_cast<HINSTANCE>(::GetModuleHandle(nullptr));
  HICON icon = reinterpret_cast<HICON>(
      ::LoadImage(exe_instance, MAKEINTRESOURCE(icon_id), IMAGE_ICON, cx, cy,
                  LR_DEFAULTCOLOR));
  if (!icon) {
    HRESULT hr = HRESULTFromLastError();
    VLOG(1) << "SetWindowIcon - LoadImage failed" << hr;
    return hr;
  }

  ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
  *hicon = icon;
  return S_OK;
}

std::wstring GetInstallerDisplayName(const std::u16string& bundle_name) {
  std::wstring display_name = base::AsWString(bundle_name);
  if (display_name.empty()) {
    display_name = GetLocalizedString(IDS_FRIENDLY_COMPANY_NAME_BASE);
  }
  return GetLocalizedStringF(IDS_INSTALLER_DISPLAY_NAME_BASE, display_name);
}

bool GetDlgItemText(HWND dlg, int item_id, std::wstring* text) {
  text->clear();
  auto* item = ::GetDlgItem(dlg, item_id);
  if (!item) {
    return false;
  }
  const auto num_chars = ::GetWindowTextLength(item);
  if (!num_chars) {
    return false;
  }
  std::vector<wchar_t> tmp(num_chars + 1);
  if (!::GetWindowText(item, &tmp.front(), tmp.size())) {
    return false;
  }
  text->assign(tmp.begin(), tmp.end());
  return true;
}

bool IsHighContrastOn() {
  HIGHCONTRAST hc = {0};
  hc.cbSize = sizeof(HIGHCONTRAST);
  if (!::SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &hc, 0)) {
    return false;
  }
  return hc.dwFlags & HCF_HIGHCONTRASTON;
}

}  // namespace updater::ui
