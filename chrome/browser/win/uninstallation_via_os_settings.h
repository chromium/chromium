// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_UNINSTALLATION_VIA_OS_SETTINGS_H_
#define CHROME_BROWSER_WIN_UNINSTALLATION_VIA_OS_SETTINGS_H_

#include <string>

namespace base {

class CommandLine;
class FilePath;

}  // namespace base

// Registers an uninstall command for a program |key| with the
// Windows Programs and Features control panel.
// The API doesn't allow same duplicate entry for the |key|. If the caller
// updates the uninstall string, it should delete the previous one first.
// |key|: A command ID that is a representative of the application.
// |display_name|: Display name that will be shown on App or Remove Program.
// |publisher|: URL Origin name of the App where it is installed from.
// |uninstall_command|: A command line that has command string that is run
// when the uninstall command is executed for the target application.
// |icon_path|: An icon that will be shown with name in the uninstall UI
// such as Program and Features.
// The system default icon will be used if |icon_path| is empty.
bool RegisterUninstallationViaOsSettings(
    const std::wstring& key,
    const std::wstring& display_name,
    const std::wstring& publisher,
    const base::CommandLine& uninstall_command,
    const base::FilePath& icon_path);

// Removes the uninstall command for the program `key`. `key` should be
// same with what is used for RegisterUninstallationViaOsSettings for the entry.
// Returns true if no error occurred. The registry entry not existing in the
// first place is considered a success.
bool UnregisterUninstallationViaOsSettings(const std::wstring& key);

#endif  // CHROME_BROWSER_WIN_UNINSTALLATION_VIA_OS_SETTINGS_H_
