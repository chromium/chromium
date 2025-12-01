// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

WebAppInstallManager::WebAppInstallManager(Profile* profile)
    : profile_(profile) {}

WebAppInstallManager::~WebAppInstallManager() {
  NotifyWebAppInstallManagerDestroyed();
}

void WebAppInstallManager::SetProvider(base::PassKey<WebAppProvider>,
                                       WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppInstallManager::Start() {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    error_log_ = std::make_unique<PersistableLog>(
        PersistableLog::GetLogPath(profile_, "InstallManager.log"),
        PersistableLog::GetMode(), PersistableLog::GetMaxInMemoryLogEntries(),
        provider_->file_utils());
  }
}

void WebAppInstallManager::Shutdown() {}

PersistableLog* WebAppInstallManager::error_log() const {
  return error_log_.get();
}

void WebAppInstallManager::TakeCommandErrorLog(
    base::PassKey<WebAppCommandManager>,
    base::Value log) {
  error_log_->Append(std::move(log));
}

void WebAppInstallManager::AddObserver(WebAppInstallManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppInstallManager::RemoveObserver(
    WebAppInstallManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppInstallManager::NotifyWebAppInstalled(const webapps::AppId& app_id) {
  DVLOG(1) << "NotifyWebAppInstalled " << app_id;
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppInstalled(app_id);
  }
  // TODO(alancutter): Call RecordWebAppInstallation here when we get access to
  // the webapps::WebappInstallSource in this event.
}

void WebAppInstallManager::NotifyWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  DVLOG(1) << "NotifyWebAppInstalledWithOsHooks " << app_id;
  for (WebAppInstallManagerObserver& obs : observers_) {
    obs.OnWebAppInstalledWithOsHooks(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppSourceRemoved(
    const webapps::AppId& app_id) {
  DVLOG(1) << "NotifyWebAppSourceRemoved " << app_id;
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppSourceRemoved(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  DVLOG(1) << "NotifyWebAppUninstalled " << app_id;
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppUninstalled(app_id, uninstall_source);
  }
}

void WebAppInstallManager::NotifyWebAppManifestUpdated(
    const webapps::AppId& app_id) {
  DVLOG(1) << "NotifyWebAppManifestUpdated " << app_id;
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppManifestUpdated(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  DVLOG(1) << "NotifyWebAppWillBeUninstalled " << app_id;
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppWillBeUninstalled(app_id);
  }
  RecordWebAppUninstallation(profile_->GetPrefs(), app_id);
}

void WebAppInstallManager::NotifyWebAppInstallManagerDestroyed() {
  DVLOG(1) << "NotifyWebAppInstallManagerDestroyed";
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppInstallManagerDestroyed();
  }
}

}  // namespace web_app
