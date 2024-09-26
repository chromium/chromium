// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace webapps {
enum class InstallResultCode;
}

namespace web_app {

struct ExternalInstallOptions;
class WebAppInstallManager;

// For InstallWebAppFromInfo see web_app_install_test_utils.h

// Navigates to |app_url| and installs app without any installability checks.
// Always selects to open app in its own window.
webapps::AppId InstallWebAppFromPage(Browser* browser, const GURL& app_url);

// Same as InstallWebAppFromPage() but waits for the app browser window to
// appear and closes it.
webapps::AppId InstallWebAppFromPageAndCloseAppBrowser(Browser* browser,
                                                       const GURL& app_url);

// Navigates to |app_url|, verifies WebApp installability, and installs app.
webapps::AppId InstallWebAppFromManifest(Browser* browser, const GURL& app_url);

// Launches a new app window for |app| in |profile| with specified
// |disposition|.
Browser* LaunchWebAppBrowser(
    Profile*,
    const webapps::AppId&,
    WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

// Launches the app, waits for the app url to load.
Browser* LaunchWebAppBrowserAndWait(
    Profile*,
    const webapps::AppId&,
    WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

// Launches a new tab for |app| in |profile|.
Browser* LaunchBrowserForWebAppInTab(
    Profile*,
    const webapps::AppId&,
    WindowOpenDisposition disposition =
        WindowOpenDisposition::NEW_FOREGROUND_TAB);

// Launches the web app to the given URL.
Browser* LaunchWebAppToURL(Profile* profile,
                           const webapps::AppId& app_id,
                           const GURL& url);

// Return |ExternalInstallOptions| with OS shortcut creation disabled.
ExternalInstallOptions CreateInstallOptions(
    const GURL& url,
    const ExternalInstallSource& source =
        ExternalInstallSource::kInternalDefault);

// Synchronous version of ExternallyManagedAppManager::Install.
ExternallyManagedAppManager::InstallResult ExternallyManagedAppManagerInstall(
    Profile*,
    ExternalInstallOptions);

// This function simulates loading a given url via a link click.
// If |proceed_through_interstitial| is true, asserts that a security
// interstitial is shown, and clicks through it, before returning.
// Note - this does NOT wait for the given url to load, it just waits for
// navigation to complete. To ensure the given url is fully loaded, wait for
// that separately.
void NavigateViaLinkClickToURLAndWait(
    Browser* browser,
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

// Searches for a Browser window for a given |app_id|. browser->app_name() must
// be defined.
Browser* FindWebAppBrowser(Profile* profile, const webapps::AppId& app_id);

void CloseAndWait(Browser* browser);

bool IsBrowserOpen(const Browser* test_browser);

// Install a web policy app with |url|.
// Returns a valid app ID of the installed app or nullopt.
std::optional<webapps::AppId> ForceInstallWebApp(Profile* profile, GURL url);

// Helper class that lets you await one Browser added and one Browser removed
// event. Optionally filters to a specific Browser with |filter|. Useful for
// closing the web app window that appears after installation from page.
class BrowserWaiter : public BrowserListObserver {
 public:
  explicit BrowserWaiter(Browser* filter = nullptr);
  ~BrowserWaiter() override;

  Browser* AwaitAdded(
      const base::Location& location = base::Location::Current());
  Browser* AwaitRemoved(
      const base::Location& location = base::Location::Current());

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  const raw_ptr<Browser, AcrossTasksDanglingUntriaged> filter_ = nullptr;

  base::RunLoop added_run_loop_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> added_browser_ = nullptr;

  base::RunLoop removed_run_loop_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> removed_browser_ = nullptr;
};

class UpdateAwaiter : public WebAppInstallManagerObserver {
 public:
  explicit UpdateAwaiter(WebAppInstallManager& install_manager);
  ~UpdateAwaiter() override;
  void AwaitUpdate(const base::Location& location = base::Location::Current());

  // WebAppInstallManagerObserver:
  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      scoped_observation_{this};
};

// Creates a temporary file with the |extension|.
base::FilePath CreateTestFileWithExtension(std::string_view extension);

// Wait for an IPH bubble to show up inside the browser, and return true or
// false based on whether the bubble showed up.
bool WaitForIPHToShowIfAny(Browser* browser);

namespace test {

// Denote ways to simulate click on an element.
enum class ClickMethod {
  kLeftClick,
  kMiddleClick,
  kShiftClick,
  kRightClickLaunchApp
};

// This function simulates a click on the middle of an element matching
// `element_id` based on the type of click passed to it.
void SimulateClickOnElement(content::WebContents* contents,
                            std::string element_id,
                            ClickMethod click);

}  // namespace test

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_BROWSERTEST_UTIL_H_
