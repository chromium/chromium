// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
#define CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

enum class WindowOpenDisposition;

// Opens the application, possibly prompting the user to re-enable it.
void OpenApplicationWithReenablePrompt(Profile* profile,
                                       apps::AppLaunchParams&& params);

// Open the application in a way specified by |params|.
// Result may be nullptr if Navigate() fails.
content::WebContents* OpenApplication(Profile* profile,
                                      apps::AppLaunchParams&& params);

// Create the application in a way specified by |params| in a new window but
// delaying activating and showing it.
Browser* CreateApplicationWindow(Profile* profile,
                                 const apps::AppLaunchParams& params,
                                 const GURL& url);

// Navigate application window to application url, but do not show it yet.
content::WebContents* NavigateApplicationWindow(
    Browser* browser,
    const apps::AppLaunchParams& params,
    const GURL& url,
    WindowOpenDisposition disposition);

// Open the application in a way specified by |params| in a new window.
// Returns nullptr if a browser window cannot be opened.
content::WebContents* OpenApplicationWindow(Profile* profile,
                                            const apps::AppLaunchParams& params,
                                            const GURL& url);

// Open |url| in an app shortcut window.
// There are two kinds of app shortcuts: Shortcuts to a URL,
// and shortcuts that open an installed application.  This function
// is used to open the former.  To open the latter, use
// application_launch::OpenApplication().
content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url);

// Whether the extension can be launched by sending a
// chrome.app.runtime.onLaunched event.
bool CanLaunchViaEvent(const extensions::Extension* extension);

// Attempt to open |app_id| in a new window or tab. Open an empty browser
// window if unsuccessful. The user's preferred launch container for the app
// (standalone window or browser tab) is used. |callback| will be called with
// the container type used to open the app, kLaunchContainerNone if an empty
// browser window was opened.
void LaunchAppWithCallback(
    Profile* profile,
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    base::OnceCallback<void(Browser* browser, apps::LaunchContainer container)>
        callback);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Shows the browser for |profile| if existent, otherwise attempts to open it.
// Returns true if browser window already exists or if it was successfully
// launched.
bool ShowBrowserForProfile(Profile* profile,
                           const apps::AppLaunchParams& params);
#endif

#endif  // CHROME_BROWSER_UI_EXTENSIONS_APPLICATION_LAUNCH_H_
