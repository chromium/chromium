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
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;
class Profile;
namespace apps {
struct ShareTarget;
}
namespace base {
class Time;
}
// Forward declared to support safe downcast;
namespace extensions {
class BookmarkAppRegistrar;
}

namespace web_app {

class AppRegistrarObserver;
class WebAppRegistrar;
class WebApp;
class OsIntegrationManager;

enum class ExternalInstallSource;

class AppRegistrar {
 public:
  explicit AppRegistrar(Profile* profile);
  virtual ~AppRegistrar();

  virtual void Start() {}
  virtual void Shutdown() {}

  // Returns whether the app with |app_id| is currently listed in the registry.
  // ie. we have data for web app manifest and icons, and this |app_id| can be
  // used in other registrar methods.
  virtual bool IsInstalled(const AppId& app_id) const = 0;

  // Returns whether the app with |app_id| is currently fully locally installed.
  // ie. app is not grey in chrome://apps UI surface and may have OS integration
  // like shortcuts. |IsLocallyInstalled| apps is a subset of |IsInstalled|
  // apps. On Chrome OS all apps are always locally installed.
  virtual bool IsLocallyInstalled(const AppId& app_id) const = 0;

  // Returns true if the app was installed by user, false if default installed.
  virtual bool WasInstalledByUser(const AppId& app_id) const = 0;

  // Returns true if the app was installed by the device OEM. Always false on
  // on non-Chrome OS.
  virtual bool WasInstalledByOem(const AppId& app_id) const = 0;

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
  // |app_id| from any |install_source|.
  virtual bool HasExternalApp(const AppId& app_id) const;

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
  virtual base::Optional<SkColor> GetAppBackgroundColor(
      const AppId& app_id) const = 0;
  virtual const GURL& GetAppStartUrl(const AppId& app_id) const = 0;
  virtual const std::string* GetAppLaunchQueryParams(
      const AppId& app_id) const = 0;
  virtual const apps::ShareTarget* GetAppShareTarget(
      const AppId& app_id) const = 0;
  virtual blink::mojom::CaptureLinks GetAppCaptureLinks(
      const AppId& app_id) const = 0;
  virtual const apps::FileHandlers* GetAppFileHandlers(
      const AppId& app_id) const = 0;

  // Returns the start_url with launch_query_params appended to the end if any.
  GURL GetAppLaunchUrl(const AppId& app_id) const;

  // TODO(crbug.com/910016): Replace uses of this with GetAppScope().
  virtual base::Optional<GURL> GetAppScopeInternal(
      const AppId& app_id) const = 0;

  virtual DisplayMode GetAppDisplayMode(const AppId& app_id) const = 0;
  virtual DisplayMode GetAppUserDisplayMode(const AppId& app_id) const = 0;
  virtual std::vector<DisplayMode> GetAppDisplayModeOverride(
      const AppId& app_id) const = 0;

  // Returns the "url_handlers" field from the app manifest.
  virtual apps::UrlHandlers GetAppUrlHandlers(const AppId& app_id) const = 0;

  virtual GURL GetAppManifestUrl(const AppId& app_id) const = 0;

  virtual base::Time GetAppLastBadgingTime(const AppId& app_id) const = 0;
  virtual base::Time GetAppLastLaunchTime(const AppId& app_id) const = 0;
  virtual base::Time GetAppInstallTime(const AppId& app_id) const = 0;

  // Returns the "icons" field from the app manifest, use |AppIconManager| to
  // load icon bitmap data.
  virtual std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const AppId& app_id) const = 0;

  // Represents which icon sizes we successfully downloaded from the IconInfos.
  virtual SortedSizesPx GetAppDownloadedIconSizesAny(
      const AppId& app_id) const = 0;

  // Returns the "shortcuts" field from the app manifest, use |AppIconManager|
  // to load shortcuts menu icons bitmaps data.
  virtual std::vector<WebApplicationShortcutsMenuItemInfo>
  GetAppShortcutsMenuItemInfos(const AppId& app_id) const = 0;

  // Returns the Run on OS Login mode.
  virtual RunOnOsLoginMode GetAppRunOnOsLoginMode(
      const AppId& app_id) const = 0;

  // Represents which icon sizes we successfully downloaded from the
  // ShortcutsMenuItemInfos.
  virtual std::vector<IconSizes> GetAppDownloadedShortcutsMenuIconsSizes(
      const AppId& app_id) const = 0;

  virtual std::vector<AppId> GetAppIds() const = 0;

  // Safe downcast.
  virtual WebAppRegistrar* AsWebAppRegistrar() = 0;
  virtual extensions::BookmarkAppRegistrar* AsBookmarkAppRegistrar();

  void SetSubsystems(OsIntegrationManager* os_integration_manager);

  // Returns the "scope" field from the app manifest, or infers a scope from the
  // "start_url" field if unavailable. Returns an invalid GURL iff the |app_id|
  // does not refer to an installed web app.
  GURL GetAppScope(const AppId& app_id) const;

  // Returns the app id of an app in the registry with the longest scope that is
  // a prefix of |url|, if any.
  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url) const;

  // Returns true if there exists at least one app installed under |scope|.
  bool DoesScopeContainAnyApp(const GURL& scope) const;

  // Finds all apps that are installed under |scope|.
  std::vector<AppId> FindAppsInScope(const GURL& scope) const;

  // Returns the app id of an installed app in the registry with the longest
  // scope that is a prefix of |url|, if any. If |window_only| is specified,
  // only apps that open in app windows will be considered.
  base::Optional<AppId> FindInstalledAppWithUrlInScope(
      const GURL& url,
      bool window_only = false) const;

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

  // Computes and returns the DisplayMode, accounting for user preference
  // to launch in a browser window and entries in the web app manifest.
  DisplayMode GetAppEffectiveDisplayMode(const AppId& app_id) const;

  // Computes and returns the DisplayMode only accounting for
  // entries in the web app manifest.
  DisplayMode GetEffectiveDisplayModeFromManifest(const AppId& app_id) const;

  // TODO(crbug.com/897314): Finish experiment by legitimising it as a
  // DisplayMode or removing entirely.
  bool IsInExperimentalTabbedWindowMode(const AppId& app_id) const;

  void AddObserver(AppRegistrarObserver* observer);
  void RemoveObserver(AppRegistrarObserver* observer);

  void NotifyWebAppInstalled(const AppId& app_id);
  void NotifyWebAppManifestUpdated(const AppId& app_id,
                                   base::StringPiece old_name);
  void NotifyWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state);
  void NotifyWebAppUninstalled(const AppId& app_id);
  void NotifyWebAppWillBeUninstalled(const AppId& app_id);
  void NotifyWebAppLocallyInstalledStateChanged(const AppId& app_id,
                                                bool is_locally_installed);
  void NotifyWebAppDisabledStateChanged(const AppId& app_id, bool is_disabled);
  void NotifyWebAppsDisabledModeChanged();
  void NotifyWebAppLastLaunchTimeChanged(const AppId& app_id,
                                         const base::Time& time);
  void NotifyWebAppInstallTimeChanged(const AppId& app_id,
                                      const base::Time& time);

  // Notify when OS hooks installation is finished during Web App installation.
  void NotifyWebAppInstalledWithOsHooks(const AppId& app_id);

 protected:
  Profile* profile() const { return profile_; }
  OsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }

  void NotifyWebAppProfileWillBeDeleted(const AppId& app_id);
  void NotifyAppRegistrarShutdown();

 private:
  Profile* const profile_;

  base::ObserverList<AppRegistrarObserver, /*check_empty=*/true> observers_;
  OsIntegrationManager* os_integration_manager_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
