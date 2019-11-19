// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UTIL_H_
#define CHROME_UPDATER_WIN_UI_UTIL_H_

#include <windows.h>

#include <stdint.h>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace updater {
namespace ui {

// Finds all the primary windows owned by the given process. A primary window is
// a top-level, has a system menu, and it is visible.
// Flags for window requirements.
constexpr uint32_t kWindowMustBeTopLevel = 0x00000001;
constexpr uint32_t kWindowMustHaveSysMenu = 0x00000002;
constexpr uint32_t kWindowMustBeVisible = 0x00000004;
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
base::string16 GetInstallerDisplayName(const base::string16& bundle_name);

// Returns the quotient of the two numbers (m/n) rounded upwards to the
// nearest integer type. T should be unsigned integer type, such as unsigned
// short, unsigned long, unsigned int etc.
template <typename T>
inline T CeilingDivide(T m, T n) {
  DCHECK_NE(0, n);
  return (m + n - 1) / n;
}

// Loads a string from the resources.
bool LoadString(int id, base::string16* s);

// Gets the text corresponding to a control in a dialog box.
bool GetDlgItemText(HWND dlg, int item_id, base::string16* text);

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_UTIL_H_
