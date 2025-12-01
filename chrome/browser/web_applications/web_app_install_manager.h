// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>

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

class PersistableLog;
class WebAppCommandManager;
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

  // Install manager error log, which is only populated if the user has enabled
  // extra logging via chrome://flags/#record-web-app-debug-info. Otherwise this
  // returns a nullptr.
  //
  // The logs are stored in memory and also persisted to a file in the user's
  // profile directory. This log is used to display debug information on the
  // chrome://web-app-internals page.
  PersistableLog* error_log() const;

  // TODO(crbug.com/40224498): Migrate logging to WebAppCommandManager after all
  // tasks are migrated to the command system, and then remove this.
  void TakeCommandErrorLog(base::PassKey<WebAppCommandManager>,
                           base::Value log);

 private:
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  // TODO(crbug.com/40224498): Remove this after install logging is fully
  // migrated to command logging.
  std::unique_ptr<PersistableLog> error_log_;

  base::ObserverList<WebAppInstallManagerObserver, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
