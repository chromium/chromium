// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_

#include <map>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;
class Profile;

namespace web_app {

class AppRegistrarObserver;

enum class ExternalInstallSource;

class AppRegistrar {
 public:
  explicit AppRegistrar(Profile* profile);
  virtual ~AppRegistrar();

  // Returns true if the app with |app_id| is currently installed.
  virtual bool IsInstalled(const AppId& app_id) const = 0;

  // Returns true if the app with |app_id| is currently fully locally installed.
  virtual bool IsLocallyInstalled(const AppId& app_id) const = 0;

  // Returns true if the app with |app_id| was previously uninstalled by the
  // user. For example, if a user uninstalls a default app ('default apps' are
  // considered external apps), then this will return true.
  virtual bool WasExternalAppUninstalledByUser(const AppId& app_id) const = 0;

  // Returns true if the app was installed by user, false if default installed.
  virtual bool WasInstalledByUser(const AppId& app_id) const = 0;

  // Returns the AppIds and URLs of apps externally installed from
  // |install_source|.
  virtual std::map<AppId, GURL> GetExternallyInstalledApps(
      ExternalInstallSource install_source) const;

  // Returns the app id for |install_url| if the AppRegistrar is aware of an
  // externally installed app for it. Note that the |install_url| is the URL
  // that the app was installed from, which may not necessarily match the app's
  // current start URL.
  virtual base::Optional<AppId> LookupExternalAppId(
      const GURL& install_url) const;

  // Returns whether the AppRegistrar has an externally installed app with
  // |app_id| from |install_source|.
  virtual bool HasExternalAppWithInstallSource(
      const AppId& app_id,
      ExternalInstallSource install_source) const;

  // Count a number of all apps which are installed by user (non-default).
  // Requires app registry to be in a ready state.
  virtual int CountUserInstalledApps() const = 0;

  // All names are UTF8 encoded.
  virtual std::string GetAppShortName(const AppId& app_id) const = 0;
  virtual std::string GetAppDescription(const AppId& app_id) const = 0;
  virtual base::Optional<SkColor> GetAppThemeColor(
      const AppId& app_id) const = 0;
  virtual const GURL& GetAppLaunchURL(const AppId& app_id) const = 0;
  virtual base::Optional<GURL> GetAppScope(const AppId& app_id) const = 0;
  virtual DisplayMode GetAppDisplayMode(const AppId& app_id) const = 0;
  virtual DisplayMode GetAppUserDisplayMode(const AppId& app_id) const = 0;

  virtual std::vector<AppId> GetAppIds() const = 0;

  // Searches for the first app id in the registry for which the |url| is in
  // scope.
  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url) const;

  // Finds all apps that are installed under |scope|.
  std::vector<AppId> FindAppsInScope(const GURL& scope) const;

  // Returns whether the app is a shortcut app (as opposed to a PWA).
  bool IsShortcutApp(const AppId& app_id) const;

  // Returns true if the app with the specified |start_url| is currently fully
  // locally installed. The provided |start_url| must exactly match the launch
  // URL for the app; this method does not consult the app scope or match URLs
  // that fall within the scope.
  bool IsLocallyInstalled(const GURL& start_url) const;

  // Returns whether the app is pending successful navigation in order to
  // complete installation via the PendingAppManager.
  bool IsPlaceholderApp(const AppId& app_id) const;

  DisplayMode GetAppEffectiveDisplayMode(const AppId& app_id) const;

  void AddObserver(AppRegistrarObserver* observer);
  void RemoveObserver(AppRegistrarObserver* observer);

  void NotifyWebAppInstalled(const AppId& app_id);
  void NotifyWebAppUninstalled(const AppId& app_id);

 protected:
  Profile* profile() const { return profile_; }

  void NotifyWebAppProfileWillBeDeleted(const AppId& app_id);
  void NotifyAppRegistrarShutdown();

 private:
  Profile* const profile_;

  base::ObserverList<AppRegistrarObserver, /*check_empty=*/true> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
