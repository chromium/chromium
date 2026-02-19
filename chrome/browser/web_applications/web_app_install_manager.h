// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace base {
class Value;
}

namespace web_app {

class WebAppInstallManagerObserver;
class WebAppProvider;

class WebAppInstallManager {
 public:
  explicit WebAppInstallManager(Profile* profile);
  WebAppInstallManager(const WebAppInstallManager&) = delete;
  WebAppInstallManager& operator=(const WebAppInstallManager&) = delete;
  ~WebAppInstallManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  void AddObserver(WebAppInstallManagerObserver* observer);
  void RemoveObserver(WebAppInstallManagerObserver* observer);

  void NotifyWebAppInstalled(const webapps::AppId& app_id);
  void NotifyWebAppInstalledWithOsHooks(const webapps::AppId& app_id);
  void NotifyWebAppSourceRemoved(const webapps::AppId& app_id);
  void NotifyWebAppUninstalled(const webapps::AppId& app_id,
                               webapps::WebappUninstallSource uninstall_source);
  void NotifyWebAppManifestUpdated(const webapps::AppId& app_id);
  void NotifyWebAppWillBeUninstalled(const webapps::AppId& app_id);
  void NotifyWebAppInstallManagerDestroyed();
  void NotifyWebAppMigrated(const webapps::AppId& source_app_id,
                            const webapps::AppId& target_app_id);

 private:
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::ObserverList<WebAppInstallManagerObserver, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
