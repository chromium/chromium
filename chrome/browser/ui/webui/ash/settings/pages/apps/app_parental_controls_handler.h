// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APP_PARENTAL_CONTROLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APP_PARENTAL_CONTROLS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_parental_controls_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace ash {

namespace on_device_controls {
class AppControlsNotifier;
class BlockedAppRegistry;
}  // namespace on_device_controls

namespace settings {

// Handles communication with app parental controls UI.
// Contains cached registry of the blocked apps that needs to exists for
// the duration of the user session (guaranteed by implementation of
// `OsSettingsManager`).
class AppParentalControlsHandler
    : public app_parental_controls::mojom::AppParentalControlsHandler,
      public apps::AppRegistryCache::Observer {
 public:
  AppParentalControlsHandler(apps::AppServiceProxy* app_service_proxy,
                             Profile* profile);
  ~AppParentalControlsHandler() override;

  // app_parental_controls::mojom::AppParentalControlsHandler:
  void GetApps(GetAppsCallback callback) override;
  void UpdateApp(const std::string& id, bool is_blocked) override;
  void AddObserver(mojo::PendingRemote<
                   app_parental_controls::mojom::AppParentalControlsObserver>
                       observer) override;
  void OnControlsDisabled() override;
  void ValidatePin(const std::string& pin,
                   ValidatePinCallback callback) override;
  void SetUpPin(const std::string& pin, SetUpPinCallback callback) override;
  void VerifyPin(const std::string& pin, VerifyPinCallback callback) override;
  void IsSetupCompleted(IsSetupCompletedCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<
          app_parental_controls::mojom::AppParentalControlsHandler> receiver);

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  std::vector<app_parental_controls::mojom::AppPtr> GetAppList();
  void NotifyAppInstalledOrUpdated(app_parental_controls::mojom::AppPtr app);
  void NotifyAppRemoved(app_parental_controls::mojom::AppPtr app);

  mojo::RemoteSet<app_parental_controls::mojom::AppParentalControlsObserver>
      observer_list_;

  raw_ptr<apps::AppServiceProxy> app_service_proxy_ = nullptr;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  std::unique_ptr<on_device_controls::AppControlsNotifier>
      app_controls_notifier_;

  std::unique_ptr<on_device_controls::BlockedAppRegistry> blocked_app_registry_;

  const raw_ptr<Profile> profile_;

  mojo::Receiver<app_parental_controls::mojom::AppParentalControlsHandler>
      receiver_{this};
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APP_PARENTAL_CONTROLS_HANDLER_H_
