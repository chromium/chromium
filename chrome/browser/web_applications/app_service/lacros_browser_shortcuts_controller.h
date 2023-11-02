// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LACROS_BROWSER_SHORTCUTS_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LACROS_BROWSER_SHORTCUTS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/webapps/common/web_app_id.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For LACROS only");

class Profile;

namespace web_app {

class WebAppProvider;

// A shortcut publisher (in the App Service sense) of web app system backed
// shortcuts where the parent app is the browser.
class LacrosBrowserShortcutsController : public WebAppInstallManagerObserver {
 public:
  explicit LacrosBrowserShortcutsController(Profile* profile);
  LacrosBrowserShortcutsController(const LacrosBrowserShortcutsController&) =
      delete;
  LacrosBrowserShortcutsController& operator=(
      const LacrosBrowserShortcutsController&) = delete;
  ~LacrosBrowserShortcutsController() override;

 private:
  void Initialize();

  void InitializeOnRegistryReady();

  // Publish web app identified by `app_id` as browser shortcut to the
  // AppService if the web app is considered as shortcut in ChromeOS.
  // `raw_icon_updated` should be set when the manifest raw icon has
  // changed to allow AppService icon directory to clear the old icons.
  void MaybePublishBrowserShortcuts(const std::vector<webapps::AppId>& app_ids,
                                    bool raw_icon_updated = false);

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  const raw_ptr<Profile> profile_;

  const raw_ptr<WebAppProvider> provider_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  raw_ptr<crosapi::mojom::AppShortcutPublisher> remote_publisher_ = nullptr;
  int remote_publisher_version_ = 0;
  base::WeakPtrFactory<LacrosBrowserShortcutsController> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LACROS_BROWSER_SHORTCUTS_CONTROLLER_H_
