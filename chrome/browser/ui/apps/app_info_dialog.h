// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_

#include <string>

#include "base/callback_forward.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/gfx/native_widget_types.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Rect;
}

// TODO(tsergeant): Move these methods into a class
// Returns true if the app info dialog is available on the current platform.
bool CanPlatformShowAppInfoDialog();

// Returns true if the app info dialog is available for an app.
bool CanShowAppInfoDialog(Profile* profile, const std::string& extension_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Shows the chrome app information as a frameless window for the given |app|
// and |profile| at the given |app_info_bounds|.
void ShowAppInfoInAppList(gfx::NativeWindow parent,
                          const gfx::Rect& app_info_bounds,
                          Profile* profile,
                          const extensions::Extension* app);
#endif

// Shows the chrome app information in a native dialog box.
void ShowAppInfoInNativeDialog(content::WebContents* web_contents,
                               Profile* profile,
                               const extensions::Extension* app,
                               base::OnceClosure close_callback);

#endif  // CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
