// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace base {
class Time;
}

namespace extensions {

class Extension;

class BookmarkAppRegistrar : public web_app::AppRegistrar,
                             public ExtensionRegistryObserver {
 public:
  explicit BookmarkAppRegistrar(Profile* profile);
  ~BookmarkAppRegistrar() override;

  // AppRegistrar:
  void Start() override;
  bool IsInstalled(const web_app::AppId& app_id) const override;
  bool IsLocallyInstalled(const web_app::AppId& app_id) const override;
  bool WasInstalledByUser(const web_app::AppId& app_id) const override;
  bool WasInstalledByOem(const web_app::AppId& app_id) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const web_app::AppId& app_id) const override;
  std::string GetAppDescription(const web_app::AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(
      const web_app::AppId& app_id) const override;
  base::Optional<SkColor> GetAppBackgroundColor(
      const web_app::AppId& app_id) const override;
  const GURL& GetAppStartUrl(const web_app::AppId& app_id) const override;
  const std::string* GetAppLaunchQueryParams(
      const web_app::AppId& app_id) const override;
  const apps::ShareTarget* GetAppShareTarget(
      const web_app::AppId& app_id) const override;
  blink::mojom::CaptureLinks GetAppCaptureLinks(
      const web_app::AppId& app_id) const override;
  const apps::FileHandlers* GetAppFileHandlers(
      const web_app::AppId& app_id) const override;
  base::Optional<GURL> GetAppScopeInternal(
      const web_app::AppId& app_id) const override;
  web_app::DisplayMode GetAppDisplayMode(
      const web_app::AppId& app_id) const override;
  web_app::DisplayMode GetAppUserDisplayMode(
      const web_app::AppId& app_id) const override;
  std::vector<web_app::DisplayMode> GetAppDisplayModeOverride(
      const web_app::AppId& app_id) const override;
  apps::UrlHandlers GetAppUrlHandlers(
      const web_app::AppId& app_id) const override;
  GURL GetAppManifestUrl(const web_app::AppId& app_id) const override;
  base::Time GetAppLastBadgingTime(const web_app::AppId& app_id) const override;
  base::Time GetAppLastLaunchTime(const web_app::AppId& app_id) const override;
  base::Time GetAppInstallTime(const web_app::AppId& app_id) const override;
  std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const web_app::AppId& app_id) const override;
  SortedSizesPx GetAppDownloadedIconSizesAny(
      const web_app::AppId& app_id) const override;
  std::vector<WebApplicationShortcutsMenuItemInfo> GetAppShortcutsMenuItemInfos(
      const web_app::AppId& app_id) const override;
  std::vector<IconSizes> GetAppDownloadedShortcutsMenuIconsSizes(
      const web_app::AppId& app_id) const override;
  web_app::RunOnOsLoginMode GetAppRunOnOsLoginMode(
      const web_app::AppId& app_id) const override;
  std::vector<web_app::AppId> GetAppIds() const override;
  web_app::WebAppRegistrar* AsWebAppRegistrar() override;
  BookmarkAppRegistrar* AsBookmarkAppRegistrar() override;

  syncer::StringOrdinal GetUserPageOrdinal(const web_app::AppId& app_id) const;
  syncer::StringOrdinal GetUserLaunchOrdinal(
      const web_app::AppId& app_id) const;

  // This is the same as GetAppUserDisplayMode above except it doesn't take
  // BookmarkAppIsLocallyInstalled() flag into consideration.
  web_app::DisplayMode GetAppUserDisplayModeForMigration(
      const web_app::AppId& app_id) const;

  // ExtensionRegistryObserver:
  // OnExtensionInstalled is not handled here.
  // AppRegistrar::NotifyWebAppInstalled is triggered by
  // BookmarkAppInstallFinalizer::OnExtensionInstalled().
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // Finds the extension object in ExtensionRegistry and in the being
  // uninstalled slot.
  //
  // When AppRegistrarObserver::OnWebAppWillBeUninstalled(app_id) happens for
  // bookmark apps, the bookmark app backing that app_id is already removed
  // from ExtensionRegistry. If some abstract observer needs the extension
  // pointer for |app_id| being uninstalled, that observer should use this
  // getter. This is a short-term workaround which helps to unify
  // ExtensionsRegistry and WebAppRegistrar observation.
  const Extension* FindExtension(const web_app::AppId& app_id) const;

 private:
  // DCHECKs that app_id isn't for a Chrome app to catch places where Chrome app
  // UI accidentally starts using web_app::AppRegistrar when it shouldn't.
  const Extension* GetBookmarkAppDchecked(const web_app::AppId& app_id) const;
  const Extension* GetEnabledExtension(const web_app::AppId& app_id) const;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_observer_{this};

  // Observers may find this pointer via FindExtension method.
  const Extension* bookmark_app_being_observed_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_
