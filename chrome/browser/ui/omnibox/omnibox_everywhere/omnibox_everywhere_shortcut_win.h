// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_SHORTCUT_WIN_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_SHORTCUT_WIN_H_

#include <string>

#include "base/files/file_path.h"
#include "base/win/windows_types.h"

namespace omnibox_everywhere {

// Returns the AppUserModelId for the Omnibox Everywhere application.
std::wstring GetAppUserModelId();

// Configures the Windows taskbar, AUMID, relaunch details, and pinning
// properties for an Omnibox Everywhere widget window based on whether
// ephemeral mode is active.
void SetWindowProperties(HWND hwnd, bool is_ephemeral);

// Helper class for Windows shortcut operations that execute
// synchronously on a background COM STA thread, managed via
// base::SequenceBound<OmniboxEverywhereShortcutHelperWin>.
class OmniboxEverywhereShortcutHelperWin {
 public:
  OmniboxEverywhereShortcutHelperWin();
  ~OmniboxEverywhereShortcutHelperWin();

  OmniboxEverywhereShortcutHelperWin(
      const OmniboxEverywhereShortcutHelperWin&) = delete;
  OmniboxEverywhereShortcutHelperWin& operator=(
      const OmniboxEverywhereShortcutHelperWin&) = delete;

  // Creates/updates the Start Menu shortcut for Omnibox Everywhere with
  // AppUserModelId, icon resource, and relaunch command.
  // Must be run on a COM STA thread supporting blocking I/O.
  bool CreateStartMenuShortcut();
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_SHORTCUT_WIN_H_
