// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_BROWSER_SHORTCUTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_BROWSER_SHORTCUTS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/webapps/common/web_app_id.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ash only");

class Profile;

namespace ui {
enum ResourceScaleFactor : int;
}

namespace web_app {

class WebAppProvider;

// A shortcut publisher (in the App Service sense) of web app system backed
// shortcuts where the parent app is the browser.
class BrowserShortcuts : public apps::ShortcutPublisher,
                         public base::SupportsWeakPtr<BrowserShortcuts>,
                         public WebAppInstallManagerObserver,
                         public WebAppRegistrarObserver {
 public:
  explicit BrowserShortcuts(apps::AppServiceProxy* proxy);
  BrowserShortcuts(const BrowserShortcuts&) = delete;
  BrowserShortcuts& operator=(const BrowserShortcuts&) = delete;
  ~BrowserShortcuts() override;

  static void SetInitializedCallbackForTesting(base::OnceClosure callback);

 private:
  void Initialize();

  void InitBrowserShortcuts();

  // Publish web app identified by `app_id` as browser shortcut to the
  // AppService if the web app is considered as shortcut in ChromeOS.
  // `raw_icon_updated` should be set when the manifest raw icon has
  // changed to allow AppService icon directory to clear the old icons.
  void MaybePublishBrowserShortcut(const webapps::AppId& app_id,
                                   bool raw_icon_updated = false);

  // apps::ShortcutPublisher:
  void LaunchShortcut(const std::string& host_app_id,
                      const std::string& local_id,
                      int64_t display_id) override;
  void RemoveShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      apps::UninstallSource uninstall_source) override;
  void GetCompressedIconData(const std::string& shortcut_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             apps::LoadIconCallback callback) override;

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;

  // WebAppRegistrarObserver:
  void OnAppRegistrarDestroyed() override;
  void OnWebAppUserDisplayModeChanged(
      const webapps::AppId& app_id,
      mojom::UserDisplayMode user_display_mode) override;

  const raw_ptr<Profile> profile_;

  const raw_ptr<WebAppProvider> provider_;

  raw_ptr<apps::AppServiceProxy> proxy_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver>
      registrar_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_BROWSER_SHORTCUTS_H_
