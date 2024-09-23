// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

// TODO(tsergeant): Move these methods into a class
// Returns true if the app info dialog is available on the current platform.
bool CanPlatformShowAppInfoDialog();

// Returns true if the app info dialog is available for an app.
bool CanShowAppInfoDialog(Profile* profile, const std::string& extension_id);

// Shows the chrome app information in a native dialog box.
void ShowAppInfoInNativeDialog(content::WebContents* web_contents,
                               Profile* profile,
                               const extensions::Extension* app,
                               base::OnceClosure close_callback);

#endif  // CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
