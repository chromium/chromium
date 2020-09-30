// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_

#include <memory>

#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/web_application_info.h"
#include "url/gurl.h"

class Browser;
class Profile;
struct WebApplicationInfo;

namespace web_app {

struct ExternalInstallOptions;
enum class InstallResultCode;

// Synchronous version of InstallManager::InstallWebAppFromInfo.
AppId InstallWebApp(Profile* profile, std::unique_ptr<WebApplicationInfo>);

// Navigates to |app_url|, verifies WebApp installability, and installs app.
AppId InstallWebAppFromManifest(Browser* browser, const GURL& app_url);

// Launches a new app window for |app| in |profile|.
Browser* LaunchWebAppBrowser(Profile*, const AppId&);

// Launches the app, waits for the app url to load.
Browser* LaunchWebAppBrowserAndWait(Profile*, const AppId&);

// Launches a new tab for |app| in |profile|.
Browser* LaunchBrowserForWebAppInTab(Profile*, const AppId&);

// Return |ExternalInstallOptions| with OS shortcut creation disabled.
ExternalInstallOptions CreateInstallOptions(const GURL& url);

// Synchronous version of PendingAppManager::Install.
InstallResultCode PendingAppManagerInstall(Profile*, ExternalInstallOptions);

// If |proceed_through_interstitial| is true, asserts that a security
// interstitial is shown, and clicks through it, before returning.
void NavigateToURLAndWait(Browser* browser,
                          const GURL& url,
                          bool proceed_through_interstitial = false);

// Performs a navigation and then checks that the toolbar visibility is as
// expected.
void NavigateAndCheckForToolbar(Browser* browser,
                                const GURL& url,
                                bool expected_visibility,
                                bool proceed_through_interstitial = false);

enum AppMenuCommandState {
  kEnabled,
  kDisabled,
  kNotPresent,
};

// For a non-app browser, determines if the command is enabled/disabled/absent.
AppMenuCommandState GetAppMenuCommandState(int command_id, Browser* browser);

bool IsBrowserOpen(const Browser* test_browser);

// Launches the app for |app| in |profile|.
void UninstallWebApp(Profile* profile, const AppId& app_id);

// Synchronous read of an app icon pixel.
SkColor ReadAppIconPixel(Profile* profile,
                         const AppId& app_id,
                         SquareSizePx size,
                         int x,
                         int y);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
