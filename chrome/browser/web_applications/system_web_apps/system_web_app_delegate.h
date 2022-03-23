// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace web_app {

class WebAppProvider;

using OriginTrialsMap = std::map<url::Origin, std::vector<std::string>>;

// Use #if defined to avoid compiler error on unused function.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// A convenience method to create OriginTrialsMap. Note, we only support simple
// cases for chrome:// and chrome-untrusted:// URLs. We don't support complex
// cases such as about:blank (which inherits origins from the embedding frame).
url::Origin GetOrigin(const char* url);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A class for configuring SWAs for all the out-of-application stuff. For
// example, window decorations and initial size. Clients will add a subclass for
// their application, overriding GetWebAppInfo(), and other methods as needed.
class SystemWebAppDelegate {
 public:
  // When installing via a WebAppInstallInfo, the url is never loaded. It's
  // needed only for various legacy reasons, maps for tracking state, and
  // generating the AppId and things of that nature.
  SystemWebAppDelegate(
      SystemAppType type,
      const std::string& internal_name,
      const GURL& install_url,
      Profile* profile,
      const OriginTrialsMap& origin_trials_map = OriginTrialsMap());

  SystemWebAppDelegate(const SystemWebAppDelegate& other) = delete;
  SystemWebAppDelegate& operator=(const SystemWebAppDelegate& other) = delete;
  virtual ~SystemWebAppDelegate();

  SystemAppType GetType() const { return type_; }

  // A developer-friendly name for, among other things, reporting metrics
  // and interacting with tast tests. It should follow PascalCase
  // convention, and have a corresponding entry in
  // WebAppSystemAppInternalName histogram suffixes. The internal name
  // shouldn't be changed afterwards.
  const std::string& GetInternalName() const { return internal_name_; }

  // The URL that the System App will be installed from.
  const GURL& GetInstallUrl() const { return install_url_; }

  // Returns a WebAppInstallInfo struct to complete installation.
  virtual std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const = 0;

  // If specified, the apps in |uninstall_and_replace| will have their data
  // migrated to this System App.
  virtual std::vector<AppId> GetAppIdsToUninstallAndReplace() const;

  // Minimum window size in DIPs. Empty if the app does not have a minimum.
  // TODO(https://github.com/w3c/manifest/issues/436): Replace with PWA manifest
  // properties for window size.
  virtual gfx::Size GetMinimumWindowSize() const;

  // If set, we allow only a single window for this app.
  virtual bool ShouldReuseExistingWindow() const;

  // If true, adds a "New Window" option to App's shelf context menu.
  // ShouldReuseExistingWindow() should return false at the same time.
  virtual bool ShouldShowNewWindowMenuOption() const;

  // Called when the app is launched with `params`. If the returned value is
  // non-empty, it will be passed to the page as a FileSystemDirectoryHandle
  // pre-pended to the `launchParams` list.
  virtual base::FilePath GetLaunchDirectory(
      const apps::AppLaunchParams& params) const;

  // Map from origin to enabled origin trial names for this app. For example,
  // "chrome://sample-web-app/" to ["Frobulate"]. If set, we will enable the
  // given origin trials when the corresponding origin is loaded in the app.
  const OriginTrialsMap& GetEnabledOriginTrials() const {
    return origin_trials_map_;
  }

  // Resource Ids for additional search terms.
  virtual std::vector<int> GetAdditionalSearchTerms() const;

  // If false, this app will be hidden from the Chrome OS app launcher.
  virtual bool ShouldShowInLauncher() const;

  // If false, this app will be hidden from the Chrome OS search.
  virtual bool ShouldShowInSearch() const;

  // If true, navigations (e.g. Omnibox URL, anchor link) to this app
  // will open in the app's window instead of the navigation's context (e.g.
  // browser tab).
  virtual bool ShouldCaptureNavigations() const;

  // If false, the app will non-resizeable.
  virtual bool ShouldAllowResize() const;

  // If false, the surface of app will can be non-maximizable.
  virtual bool ShouldAllowMaximize() const;

  // If true, the App's window will have a tab-strip.
  virtual bool ShouldHaveTabStrip() const;

  // If false, the app will not have the reload button in minimal ui
  // mode.
  virtual bool ShouldHaveReloadButtonInMinimalUi() const;

  // If true, allows the app to close the window through scripts, for example
  // using `window.close()`.
  virtual bool ShouldAllowScriptsToCloseWindows() const;

  // If true, allows app to show up in file-open intent and picking surfaces.
  virtual bool ShouldHandleFileOpenIntents() const;

  // Setup information to drive a background task.
  virtual absl::optional<SystemAppBackgroundTaskInfo> GetTimerInfo() const;

  // Default window bounds of the application.
  virtual gfx::Rect GetDefaultBounds(Browser* browser) const;

  // If false, the application will not be installed.
  virtual bool IsAppEnabled() const;

  // If true, GetTabMenuModel() is called to provide the tab menu model.
  virtual bool HasCustomTabMenuModel() const;

  // Optional custom tab menu model.
  virtual std::unique_ptr<ui::SimpleMenuModel> GetTabMenuModel(
      ui::SimpleMenuModel::Delegate* delegate) const;

  // Returns whether the specified Tab Context Menu shortcut should be shown.
  virtual bool ShouldShowTabContextMenuShortcut(Profile* profile,
                                                int command_id) const;
  // Control the launch of an SWA. The default takes into account single vs.
  // multiple windows, make sure multiple windows don't open directly above
  // each other, and a few other niceties. Overriding this will require some
  // knowledge of browser window and launch internals, so hopefully you'll never
  // have to roll your own here.
  //
  // If a browser is returned, app launch will continue. If false is returned,
  // it's assumed that this method has cleaned up after itself, and launch is
  // aborted.
  //
  // This is implemented in
  // chrome/browser/ui/web_applications/system_web_app_delegate_ui_impl.cc.
  virtual Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const;

  // Whether |url| which is outside the normal Navigation Scope should be
  // considered part of this System App.
  virtual bool IsUrlInSystemAppScope(const GURL& url) const;

  // Whether it is preferred to resolve background color from the manifest,
  // as opposed to resolving background color from web contents.
  virtual bool PreferManifestBackgroundColor() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns whether theme changes should be animated.
  virtual bool ShouldAnimateThemeChanges() const;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1308961): Migrate to use PWA pinned home tab when ready.
  // Returns whether the specified tab should be pinned.
  virtual bool ShouldPinTab(GURL url) const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
  Profile* profile() const { return profile_; }

  // These should all be private. See
  // https://google.github.io/styleguide/cppguide.html#Access_Control
  SystemAppType type_;
  std::string internal_name_;
  GURL install_url_;
  raw_ptr<Profile> profile_;
  OriginTrialsMap origin_trials_map_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_DELEGATE_H_
