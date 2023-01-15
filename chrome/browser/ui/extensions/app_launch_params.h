// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APP_LAUNCH_PARAMS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APP_LAUNCH_PARAMS_H_

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/base/window_open_disposition.h"

class Profile;

namespace extensions {
class Extension;
}

// Helper to create AppLaunchParams using extensions::GetLaunchContainer with
// LAUNCH_TYPE_REGULAR to check for a user-configured container.
apps::AppLaunchParams CreateAppLaunchParamsUserContainer(
    Profile* profile,
    const extensions::Extension* extension,
    WindowOpenDisposition disposition,
    apps::LaunchSource launch_source);

// Helper to create AppLaunchParams, falling back to
// extensions::GetLaunchContainer() with no modifiers.
apps::AppLaunchParams CreateAppLaunchParamsWithEventFlags(
    Profile* profile,
    const extensions::Extension* extension,
    int event_flags,
    apps::LaunchSource launch_source,
    int64_t display_id);

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APP_LAUNCH_PARAMS_H_
