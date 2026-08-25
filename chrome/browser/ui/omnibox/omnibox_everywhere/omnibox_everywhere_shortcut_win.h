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

// Returns the on-disk file path for the Omnibox Everywhere icon file.
base::FilePath GetIconFilePath();

// Configures the Windows taskbar, AUMID, relaunch details, and pinning
// properties for an Omnibox Everywhere widget window based on whether
// ephemeral mode is active.
void SetWindowProperties(HWND hwnd, bool is_ephemeral);

// Helper class for Windows shortcut and icon operations that execute
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

  // Ensures the Omnibox Everywhere .ico file is generated and persisted to disk
  // in the user data directory. If the icon already exists, this is a no-op and
  // returns true. Must be run on a thread supporting blocking file I/O.
  bool EnsureIconPersisted();

  // Creates/updates the Start Menu shortcut for Omnibox Everywhere with
  // AppUserModelId, icon, and relaunch command.
  // Must be run on a COM STA thread supporting blocking I/O.
  bool CreateStartMenuShortcut();
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_SHORTCUT_WIN_H_
