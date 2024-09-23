// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_observers.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

namespace {

#if DCHECK_IS_ON()
bool IsAnyIdEmpty(const std::set<webapps::AppId>& app_ids) {
  for (const webapps::AppId& id : app_ids) {
    if (id.empty())
      return true;
  }
  return false;
}
#endif

}  // namespace

WebAppInstallManagerObserverAdapter::WebAppInstallManagerObserverAdapter(
    WebAppInstallManager* install_manager) {
  observation_.Observe(install_manager);
}

WebAppInstallManagerObserverAdapter::WebAppInstallManagerObserverAdapter(
    Profile* profile)
    : WebAppInstallManagerObserverAdapter(
          &WebAppProvider::GetForTest(profile)->install_manager()) {}

WebAppInstallManagerObserverAdapter::~WebAppInstallManagerObserverAdapter() =
    default;

void WebAppInstallManagerObserverAdapter::SetWebAppInstalledDelegate(
    WebAppInstalledDelegate delegate) {
  app_installed_delegate_ = std::move(delegate);
}

void WebAppInstallManagerObserverAdapter::SetWebAppInstalledWithOsHooksDelegate(
    WebAppInstalledWithOsHooksDelegate delegate) {
  app_installed_with_os_hooks_delegate_ = std::move(delegate);
}

void WebAppInstallManagerObserverAdapter::SetWebAppWillBeUninstalledDelegate(
    WebAppWillBeUninstalledDelegate delegate) {
  app_will_be_uninstalled_delegate_ = std::move(delegate);
}

void WebAppInstallManagerObserverAdapter::SetWebAppUninstalledDelegate(
    WebAppUninstalledDelegate delegate) {
  app_uninstalled_delegate_ = std::move(delegate);
}

void WebAppInstallManagerObserverAdapter::SetWebAppManifestUpdateDelegate(
    WebAppManifestUpdateDelegate delegate) {
  app_manifest_updated_delegate_ = std::move(delegate);
}

void WebAppInstallManagerObserverAdapter::SetWebAppSourceRemovedDelegate(
    WebAppSourceRemovedDelegate delegate) {
  app_source_removed_delegate_ = std::move(delegate);
}

void WebAppInstallManagerObserverAdapter::OnWebAppInstalled(
    const webapps::AppId& app_id) {
  if (app_installed_delegate_)
    app_installed_delegate_.Run(app_id);
}

void WebAppInstallManagerObserverAdapter::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  if (app_installed_with_os_hooks_delegate_)
    app_installed_with_os_hooks_delegate_.Run(app_id);
}

void WebAppInstallManagerObserverAdapter::OnWebAppManifestUpdated(
    const webapps::AppId& app_id) {
  if (app_manifest_updated_delegate_)
    app_manifest_updated_delegate_.Run(app_id);
}

void WebAppInstallManagerObserverAdapter::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  if (app_will_be_uninstalled_delegate_)
    app_will_be_uninstalled_delegate_.Run(app_id);
}

void WebAppInstallManagerObserverAdapter::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (app_uninstalled_delegate_)
    app_uninstalled_delegate_.Run(app_id);
}

void WebAppInstallManagerObserverAdapter::OnWebAppInstallManagerDestroyed() {
  observation_.Reset();
}

void WebAppInstallManagerObserverAdapter::OnWebAppSourceRemoved(
    const webapps::AppId& app_id) {
  if (app_source_removed_delegate_) {
    app_source_removed_delegate_.Run(app_id);
  }
}

void WebAppInstallManagerObserverAdapter::SignalRunLoopAndStoreAppId(
    const webapps::AppId& app_id) {
  if (!is_listening_)
    return;
  optional_app_ids_.erase(app_id);
  if (!optional_app_ids_.empty())
    return;
  last_app_id_ = app_id;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, wait_loop_.QuitClosure());
  is_listening_ = false;
}

WebAppTestRegistryObserverAdapter::WebAppTestRegistryObserverAdapter(
    WebAppRegistrar* registrar) {
  observation_.Observe(registrar);
}

WebAppTestRegistryObserverAdapter::WebAppTestRegistryObserverAdapter(
    Profile* profile)
    : WebAppTestRegistryObserverAdapter(
          &WebAppProvider::GetForTest(profile)->registrar_unsafe()) {}

WebAppTestRegistryObserverAdapter::~WebAppTestRegistryObserverAdapter() =
    default;

void WebAppTestRegistryObserverAdapter::SetWebAppWillBeUpdatedFromSyncDelegate(
    WebAppWillBeUpdatedFromSyncDelegate delegate) {
  app_will_be_updated_from_sync_delegate_ = std::move(delegate);
}

void WebAppTestRegistryObserverAdapter::SetWebAppLastBadgingTimeChangedDelegate(
    WebAppLastBadgingTimeChangedDelegate delegate) {
  app_last_badging_time_changed_delegate_ = std::move(delegate);
}

void WebAppTestRegistryObserverAdapter::
    SetWebAppProtocolSettingsChangedDelegate(
        WebAppProtocolSettingsChangedDelegate delegate) {
  app_protocol_settings_changed_delegate_ = std::move(delegate);
}

void WebAppTestRegistryObserverAdapter::OnWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  if (app_will_be_updated_from_sync_delegate_)
    app_will_be_updated_from_sync_delegate_.Run(new_apps_state);
}

void WebAppTestRegistryObserverAdapter::OnWebAppLastBadgingTimeChanged(
    const webapps::AppId& app_id,
    const base::Time& time) {
  if (app_last_badging_time_changed_delegate_)
    app_last_badging_time_changed_delegate_.Run(app_id, time);
}

void WebAppTestRegistryObserverAdapter::OnWebAppProtocolSettingsChanged() {
  if (app_protocol_settings_changed_delegate_)
    app_protocol_settings_changed_delegate_.Run();
}

void WebAppTestRegistryObserverAdapter::OnAppRegistrarDestroyed() {
  observation_.Reset();
}

void WebAppTestRegistryObserverAdapter::SignalRunLoopAndStoreAppId(
    const webapps::AppId& app_id) {
  if (!is_listening_)
    return;
  optional_app_ids_.erase(app_id);
  if (!optional_app_ids_.empty())
    return;
  last_app_id_ = app_id;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, wait_loop_.QuitClosure());
  is_listening_ = false;
}

WebAppTestInstallObserver::WebAppTestInstallObserver(Profile* profile)
    : WebAppInstallManagerObserverAdapter(profile) {}
WebAppTestInstallObserver::~WebAppTestInstallObserver() = default;

void WebAppTestInstallObserver::BeginListening(
    const std::set<webapps::AppId>& optional_app_ids) {
  optional_app_ids_ = optional_app_ids;
#if DCHECK_IS_ON()
  DCHECK(!IsAnyIdEmpty(optional_app_ids_)) << "Cannot listen for empty ids.";
#endif
  is_listening_ = true;
  app_installed_delegate_ = base::BindRepeating(
      &WebAppTestInstallObserver::SignalRunLoopAndStoreAppId,
      weak_factory_.GetWeakPtr());
}

webapps::AppId WebAppTestInstallObserver::Wait() {
  wait_loop_.Run();
  return last_app_id_;
}

webapps::AppId WebAppTestInstallObserver::BeginListeningAndWait(
    const std::set<webapps::AppId>& optional_app_ids) {
  BeginListening(optional_app_ids);
  webapps::AppId id = Wait();
  return id;
}

WebAppTestInstallWithOsHooksObserver::WebAppTestInstallWithOsHooksObserver(
    Profile* profile)
    : WebAppInstallManagerObserverAdapter(profile) {}
WebAppTestInstallWithOsHooksObserver::~WebAppTestInstallWithOsHooksObserver() =
    default;

void WebAppTestInstallWithOsHooksObserver::BeginListening(
    const std::set<webapps::AppId>& optional_app_ids) {
  optional_app_ids_ = optional_app_ids;
#if DCHECK_IS_ON()
  DCHECK(!IsAnyIdEmpty(optional_app_ids_)) << "Cannot listen for empty ids.";
#endif
  is_listening_ = true;
  app_installed_with_os_hooks_delegate_ = base::BindRepeating(
      &WebAppTestInstallWithOsHooksObserver::SignalRunLoopAndStoreAppId,
      weak_factory_.GetWeakPtr());
}

webapps::AppId WebAppTestInstallWithOsHooksObserver::Wait() {
  wait_loop_.Run();
  return last_app_id_;
}

webapps::AppId WebAppTestInstallWithOsHooksObserver::BeginListeningAndWait(
    const std::set<webapps::AppId>& optional_app_ids) {
  BeginListening(optional_app_ids);
  webapps::AppId id = Wait();
  return id;
}

WebAppTestManifestUpdatedObserver::WebAppTestManifestUpdatedObserver(
    WebAppInstallManager* install_manager)
    : WebAppInstallManagerObserverAdapter(install_manager) {}
WebAppTestManifestUpdatedObserver::~WebAppTestManifestUpdatedObserver() =
    default;

void WebAppTestManifestUpdatedObserver::BeginListening(
    const std::set<webapps::AppId>& optional_app_ids) {
  optional_app_ids_ = optional_app_ids;
#if DCHECK_IS_ON()
  DCHECK(!IsAnyIdEmpty(optional_app_ids_)) << "Cannot listen for empty ids.";
#endif
  is_listening_ = true;
  app_manifest_updated_delegate_ = base::BindRepeating(
      &WebAppTestManifestUpdatedObserver::SignalRunLoopAndStoreAppId,
      weak_factory_.GetWeakPtr());
}

webapps::AppId WebAppTestManifestUpdatedObserver::Wait() {
  wait_loop_.Run();
  return last_app_id_;
}

webapps::AppId WebAppTestManifestUpdatedObserver::BeginListeningAndWait(
    const std::set<webapps::AppId>& optional_app_ids) {
  BeginListening(optional_app_ids);
  webapps::AppId id = Wait();
  return id;
}

WebAppTestUninstallObserver::WebAppTestUninstallObserver(Profile* profile)
    : WebAppInstallManagerObserverAdapter(profile) {}

WebAppTestUninstallObserver::~WebAppTestUninstallObserver() = default;

void WebAppTestUninstallObserver::BeginListening(
    const std::set<webapps::AppId>& optional_app_ids) {
  optional_app_ids_ = optional_app_ids;
#if DCHECK_IS_ON()
  DCHECK(!IsAnyIdEmpty(optional_app_ids_)) << "Cannot listen for empty ids.";
#endif
  is_listening_ = true;
  app_uninstalled_delegate_ = base::BindRepeating(
      &WebAppTestUninstallObserver::SignalRunLoopAndStoreAppId,
      weak_factory_.GetWeakPtr());
}

webapps::AppId WebAppTestUninstallObserver::Wait() {
  wait_loop_.Run();
  return last_app_id_;
}

webapps::AppId WebAppTestUninstallObserver::BeginListeningAndWait(
    const std::set<webapps::AppId>& optional_app_ids) {
  BeginListening(optional_app_ids);
  webapps::AppId id = Wait();
  return id;
}

}  // namespace web_app
