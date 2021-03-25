// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/app_launch_params.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/arc_util.h"
#include "components/arc/arc_util.h"
#endif

using extensions::ExtensionPrefs;

apps::AppLaunchParams CreateAppLaunchParamsUserContainer(
    Profile* profile,
    const extensions::Extension* extension,
    WindowOpenDisposition disposition,
    apps::mojom::AppLaunchSource source) {
  // Look up the app preference to find out the right launch container. Default
  // is to launch as a regular tab.
  apps::mojom::LaunchContainer container =
      extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
  return apps::AppLaunchParams(extension->id(), container, disposition, source);
}

apps::AppLaunchParams CreateAppLaunchParamsWithEventFlags(
    Profile* profile,
    const extensions::Extension* extension,
    int event_flags,
    apps::mojom::AppLaunchSource source,
    int64_t display_id) {
  apps::mojom::LaunchContainer fallback_container =
      extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
  return apps::CreateAppIdLaunchParamsWithEventFlags(
      extension->id(), event_flags, source, display_id, fallback_container);
}
