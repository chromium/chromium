// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_OBSERVERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_OBSERVERS_H_

#include <set>

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

class Profile;

namespace web_app {

class WebApp;

// This is an adapter for the AppRegistrarObserver. This class registers
// itself as an observer on construction, and will call the respective
// delegates (if set) for all AppRegistrarObserver calls.
class WebAppTestRegistryObserverAdapter : public AppRegistrarObserver {
 public:
  explicit WebAppTestRegistryObserverAdapter(WebAppRegistrar* registrar);
  explicit WebAppTestRegistryObserverAdapter(Profile* profile);

  WebAppTestRegistryObserverAdapter(const WebAppTestRegistryObserverAdapter&) =
      delete;
  WebAppTestRegistryObserverAdapter& operator=(
      const WebAppTestRegistryObserverAdapter&) = delete;

  ~WebAppTestRegistryObserverAdapter() override;

  using WebAppInstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppInstalledDelegate(WebAppInstalledDelegate delegate);

  using WebAppInstalledWithOsHooksDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppInstalledWithOsHooksDelegate(
      WebAppInstalledWithOsHooksDelegate delegate);

  using WebAppWillBeUninstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppWillBeUninstalledDelegate(
      WebAppWillBeUninstalledDelegate delegate);

  using WebAppUninstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppUninstalledDelegate(WebAppUninstalledDelegate delegate);

  using WebAppProfileWillBeDeletedDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppProfileWillBeDeletedDelegate(
      WebAppProfileWillBeDeletedDelegate delegate);

  using WebAppWillBeUpdatedFromSyncDelegate = base::RepeatingCallback<void(
      const std::vector<const WebApp*>& new_apps_state)>;
  void SetWebAppWillBeUpdatedFromSyncDelegate(
      WebAppWillBeUpdatedFromSyncDelegate delegate);

  using WebAppManifestUpdateDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppManifestUpdateDelegate(WebAppManifestUpdateDelegate delegate);

  using WebAppLastBadgingTimeChangedDelegate =
      base::RepeatingCallback<void(const AppId& app_id,
                                   const base::Time& time)>;
  void SetWebAppLastBadgingTimeChangedDelegate(
      WebAppLastBadgingTimeChangedDelegate delegate);

  using WebAppProtocolSettingsChangedDelegate = base::RepeatingCallback<void()>;
  void SetWebAppProtocolSettingsChangedDelegate(
      WebAppProtocolSettingsChangedDelegate delegate);

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const AppId& app_id) override;
  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override;
  void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;
  void OnWebAppLastBadgingTimeChanged(const web_app::AppId& app_id,
                                      const base::Time& time) override;
  void OnWebAppProtocolSettingsChanged() override;

 protected:
  // Helper method for subclasses to allow easy waiting on `wait_loop_`.
  // Expects that the users set `is_listening_` to `true` and
  // optionally set `optional_app_ids_`.
  void SignalRunLoopAndStoreAppId(const AppId& app_id);

  bool is_listening_ = false;
  std::set<AppId> optional_app_ids_;
  base::RunLoop wait_loop_;
  AppId last_app_id_;

  WebAppInstalledDelegate app_installed_delegate_;
  WebAppInstalledWithOsHooksDelegate app_installed_with_os_hooks_delegate_;
  WebAppManifestUpdateDelegate app_manifest_updated_delegate_;
  WebAppUninstalledDelegate app_uninstalled_delegate_;

 private:
  WebAppWillBeUpdatedFromSyncDelegate app_will_be_updated_from_sync_delegate_;
  WebAppWillBeUninstalledDelegate app_will_be_uninstalled_delegate_;
  WebAppProfileWillBeDeletedDelegate app_profile_will_be_deleted_delegate_;
  WebAppLastBadgingTimeChangedDelegate app_last_badging_time_changed_delegate_;
  WebAppProtocolSettingsChangedDelegate app_protocol_settings_changed_delegate_;

  base::ScopedObservation<WebAppRegistrar, AppRegistrarObserver> observation_{
      this};

 protected:
  base::WeakPtrFactory<WebAppTestRegistryObserverAdapter> weak_factory_{this};
};

class WebAppTestInstallObserver final
    : public WebAppTestRegistryObserverAdapter {
 public:
  explicit WebAppTestInstallObserver(Profile* profile);
  ~WebAppTestInstallObserver() final;

  // Restricts this observer to only listen for the given
  // |optional_app_ids|. Settings these means that the
  // WebAppInstalledDelegate doesn't get called until all of the ids in
  // |optional_app_ids| are installed.
  void BeginListening(const std::set<AppId>& optional_app_ids = {});

  // Wait for the next observation (or, until all optional_app_ids are
  // observed).
  AppId Wait();

  AppId BeginListeningAndWait(const std::set<AppId>& optional_app_ids = {});
};

class WebAppTestInstallWithOsHooksObserver final
    : public WebAppTestRegistryObserverAdapter {
 public:
  explicit WebAppTestInstallWithOsHooksObserver(Profile* profile);
  ~WebAppTestInstallWithOsHooksObserver() final;

  // Restricts this observer to only listen for the given
  // |optional_app_ids|. Settings these means that the
  // WebAppInstalledWithOsHooksDelegate doesn't get called until all of the ids
  // in |optional_app_ids| are installed.
  void BeginListening(const std::set<AppId>& optional_app_ids = {});

  // Wait for the next observation (or, until all optional_app_ids are
  // observed).
  AppId Wait();

  AppId BeginListeningAndWait(const std::set<AppId>& app_ids = {});
};

class WebAppTestManifestUpdatedObserver final
    : public WebAppTestRegistryObserverAdapter {
 public:
  explicit WebAppTestManifestUpdatedObserver(WebAppRegistrar* registrar);
  ~WebAppTestManifestUpdatedObserver() final;

  // Restricts this observer to only listen for the given
  // |optional_app_ids|. Settings these means that the
  // WebAppManifestUpdateDelegate doesn't get called until all of the ids in
  // |optional_app_ids| are installed.
  void BeginListening(const std::set<AppId>& optional_app_ids = {});

  // Wait for the next observation (or, until all optional_app_ids are
  // observed).
  AppId Wait();

  AppId BeginListeningAndWait(const std::set<AppId>& app_ids = {});
};

class WebAppTestUninstallObserver final
    : public WebAppTestRegistryObserverAdapter {
 public:
  explicit WebAppTestUninstallObserver(Profile* profile);
  ~WebAppTestUninstallObserver() final;

  // Restricts this observer to only listen for the given
  // |optional_app_ids|. Settings these means that the
  // WebAppUninstalledDelegate doesn't get called until all of the ids in
  // |optional_app_ids| are installed.
  void BeginListening(const std::set<AppId>& optional_app_ids = {});

  AppId Wait();

  AppId BeginListeningAndWait(const std::set<AppId>& app_ids = {});
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_OBSERVERS_H_
