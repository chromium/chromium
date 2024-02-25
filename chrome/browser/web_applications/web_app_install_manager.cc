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
#include "chrome/browser/web_applications/web_app_internals_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

namespace {

constexpr char kWebAppInstallManagerName[] = "WebAppInstallManager";

}  // namespace

WebAppInstallManager::WebAppInstallManager(Profile* profile)
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    error_log_ = std::make_unique<ErrorLog>();
    ReadErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                 base::BindOnce(&WebAppInstallManager::OnReadErrorLog,
                                weak_ptr_factory_.GetWeakPtr()));
  } else {
    ClearErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                  base::DoNothing());
  }
}

WebAppInstallManager::~WebAppInstallManager() {
  NotifyWebAppInstallManagerDestroyed();
}

void WebAppInstallManager::Start() {}

void WebAppInstallManager::Shutdown() {}

void WebAppInstallManager::TakeCommandErrorLog(
    base::PassKey<WebAppCommandManager>,
    base::Value::Dict log) {
  if (error_log_)
    LogErrorObject(std::move(log));
}

void WebAppInstallManager::MaybeWriteErrorLog() {
  DCHECK(error_log_);
  if (error_log_writing_in_progress_ || !error_log_updated_)
    return;

  WriteErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                base::Value(error_log_->Clone()),
                base::BindOnce(&WebAppInstallManager::OnWriteErrorLog,
                               weak_ptr_factory_.GetWeakPtr()));

  error_log_writing_in_progress_ = true;
  error_log_updated_ = false;
}

void WebAppInstallManager::OnWriteErrorLog(Result result) {
  error_log_writing_in_progress_ = false;
  MaybeWriteErrorLog();
}

void WebAppInstallManager::OnReadErrorLog(Result result,
                                          base::Value error_log) {
  DCHECK(error_log_);
  if (result != Result::kOk || !error_log.is_list())
    return;

  ErrorLog early_error_log = std::move(*error_log_);
  *error_log_ = std::move(error_log).TakeList();

  // Appends the `early_error_log` at the end.
  error_log_->reserve(error_log_->size() + early_error_log.size());
  for (auto& error : early_error_log) {
    error_log_->Append(std::move(error));
  }
}

void WebAppInstallManager::LogErrorObject(base::Value::Dict object) {
  if (!error_log_)
    return;

  error_log_->Append(std::move(object));
  error_log_updated_ = true;
  MaybeWriteErrorLog();
}

void WebAppInstallManager::AddObserver(WebAppInstallManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppInstallManager::RemoveObserver(
    WebAppInstallManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppInstallManager::NotifyWebAppInstalled(const webapps::AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppInstalled(app_id);
  }
  // TODO(alancutter): Call RecordWebAppInstallation here when we get access to
  // the webapps::WebappInstallSource in this event.
}

void WebAppInstallManager::NotifyWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  for (WebAppInstallManagerObserver& obs : observers_) {
    obs.OnWebAppInstalledWithOsHooks(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppSourceRemoved(
    const webapps::AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppSourceRemoved(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppUninstalled(app_id, uninstall_source);
  }
}

void WebAppInstallManager::NotifyWebAppManifestUpdated(
    const webapps::AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppManifestUpdated(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppWillBeUninstalled(app_id);
  }
  RecordWebAppUninstallation(profile_->GetPrefs(), app_id);
}

void WebAppInstallManager::NotifyWebAppInstallManagerDestroyed() {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppInstallManagerDestroyed();
  }
}

}  // namespace web_app
