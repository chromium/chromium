// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_LAUNCH_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_LAUNCH_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// TODO(hidehiko): Revisit here how can we do better for Chrome browser on
// ChromeOS. Conceptually, this should be ok to use a part of web browser
// implementation even on ChromeOS (as a cross platform implementation),
// but should not be used as a part of OS system implementation.
static_assert(!BUILDFLAG(IS_CHROMEOS),
              "This should not be included in ChromeOS builds.");

namespace web_app {

// Launches an app for the given `app_id` in a way specified by `params`.
// This first looks up Extension by the app id, and if there is an extension,
// launches it. Otherwise, gives it a try to launch a WebApp.
// Note: The Extension part is being deprecated and probably soon removed.
// Then callers may be able to directly call into WebApps' scheduler.
// Importantly, this should NOT depend on AppServiceProxy.
void LaunchExtensionOrWebApp(
    Profile* profile,
    apps::AppLaunchParams params,
    base::OnceCallback<void(content::WebContents*)> callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_LAUNCH_H_
