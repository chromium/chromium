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
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class WebAppCommandManager;
class WebAppInstallManagerObserver;

class WebAppInstallManager {
 public:
  explicit WebAppInstallManager(Profile* profile);
  WebAppInstallManager(const WebAppInstallManager&) = delete;
  WebAppInstallManager& operator=(const WebAppInstallManager&) = delete;
  virtual ~WebAppInstallManager();

  void Start();
  void Shutdown();

  virtual void AddObserver(WebAppInstallManagerObserver* observer);
  virtual void RemoveObserver(WebAppInstallManagerObserver* observer);

  virtual void NotifyWebAppInstalled(const webapps::AppId& app_id);
  virtual void NotifyWebAppInstalledWithOsHooks(const webapps::AppId& app_id);
  virtual void NotifyWebAppSourceRemoved(const webapps::AppId& app_id);
  virtual void NotifyWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source);
  virtual void NotifyWebAppManifestUpdated(const webapps::AppId& app_id);
  virtual void NotifyWebAppWillBeUninstalled(const webapps::AppId& app_id);
  virtual void NotifyWebAppInstallManagerDestroyed();

  // Collects icon read/write errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  using ErrorLog = base::Value::List;
  const ErrorLog* error_log() const { return error_log_.get(); }

  // TODO(crbug.com/40224498): migrate loggign to WebAppCommandManager after all
  // tasks are migrated to the command system.
  void TakeCommandErrorLog(base::PassKey<WebAppCommandManager>,
                           base::Value::Dict log);

 private:
  void MaybeWriteErrorLog();
  void OnWriteErrorLog(Result result);
  void OnReadErrorLog(Result result, base::Value error_log);

  void LogErrorObject(base::Value::Dict object);

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  std::unique_ptr<ErrorLog> error_log_;
  bool error_log_updated_ = false;
  bool error_log_writing_in_progress_ = false;

  base::ObserverList<WebAppInstallManagerObserver, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
