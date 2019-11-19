// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_MAC_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"

class Profile;

namespace base {
class CommandLine;
}

namespace extensions {
class Extension;
}

namespace web_app {

// Rebuild the shortcut and relaunch it.
bool MaybeRebuildShortcut(const base::CommandLine& command_line);

// Reveals app shim in Finder given a profile and app.
// Calls RevealAppShimInFinderForAppOnFileThread and schedules it
// on the FILE thread.
void RevealAppShimInFinderForApp(Profile* profile,
                                 const extensions::Extension* app);

// Callback made by GetProfilesForAppShim.
using GetProfilesForAppShimCallback =
    base::OnceCallback<void(const std::vector<base::FilePath>&)>;

// Call |callback| with the subset of |profile_paths_to_check| for which the app
// with |app_id| in installed.
void GetProfilesForAppShim(
    const std::string& app_id,
    const std::vector<base::FilePath>& profile_paths_to_check,
    GetProfilesForAppShimCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_MAC_H_
