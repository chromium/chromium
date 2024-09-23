// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_UTIL_H_
#define CHROME_UPDATER_WIN_UI_UI_UTIL_H_

#include <windows.h>

#include <stdint.h>

#include <string>
#include <vector>

namespace updater::ui {

// Finds all the primary windows owned by the given process. A primary window is
// a top-level, has a system menu, and it is visible.
// Flags for window requirements.
inline constexpr uint32_t kWindowMustBeTopLevel = 0x00000001;
inline constexpr uint32_t kWindowMustHaveSysMenu = 0x00000002;
inline constexpr uint32_t kWindowMustBeVisible = 0x00000004;
bool FindProcessWindows(uint32_t process_id,
                        uint32_t window_flags,
                        std::vector<HWND>* windows);

// Forces the window to the foreground.
void MakeWindowForeground(HWND wnd);

// Returns true if the window is the main window of a process. That means that
// the window is visible and it is a top level window.
bool IsMainWindow(HWND wnd);

// Returns true if the window has a system menu.
bool HasSystemMenu(HWND wnd);

// Sets the icon of a window icon given the |icon_id| in the resources of
// the EXE module.
HRESULT SetWindowIcon(HWND hwnd, WORD icon_id, HICON* hicon);

// Returns a localized installer name for a bundle. If |bundle_name| is empty,
// the friendly company name is used.
std::wstring GetInstallerDisplayName(const std::u16string& bundle_name);

// Gets the text corresponding to a control in a dialog box.
bool GetDlgItemText(HWND dlg, int item_id, std::wstring* text);

// Returns true if the system is in high contrast mode.
bool IsHighContrastOn();

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_UTIL_H_
