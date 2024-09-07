// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAST_CONFIG_CAST_CONFIG_CONTROLLER_MEDIA_ROUTER_H_
#define CHROME_BROWSER_UI_ASH_CAST_CONFIG_CAST_CONFIG_CONTROLLER_MEDIA_ROUTER_H_

#include <memory>

#include "ash/public/cpp/cast_config_controller.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace media_router {
class MediaRouter;
}

class AccountId;
class CastDeviceCache;

// A class which allows the ash tray to communicate with the media router.
class CastConfigControllerMediaRouter
    : public ash::CastConfigController,
      public session_manager::SessionManagerObserver,
      public media_router::MirroringMediaControllerHost::Observer {
 public:
  CastConfigControllerMediaRouter();

  CastConfigControllerMediaRouter(const CastConfigControllerMediaRouter&) =
      delete;
  CastConfigControllerMediaRouter& operator=(
      const CastConfigControllerMediaRouter&) = delete;

  ~CastConfigControllerMediaRouter() override;

  // media_router::MirroringMediaControllerHost::Observer:
  void OnFreezeInfoChanged() override;

  static void SetMediaRouterForTest(media_router::MediaRouter* media_router);

 private:
  // CastConfigController:
  void AddObserver(CastConfigController::Observer* observer) override;
  void RemoveObserver(CastConfigController::Observer* observer) override;
  bool HasMediaRouterForPrimaryProfile() const override;
  bool HasSinksAndRoutes() const override;
  bool HasActiveRoute() const override;
  bool AccessCodeCastingEnabled() const override;
  void RequestDeviceRefresh() override;
  const std::vector<ash::SinkAndRoute>& GetSinksAndRoutes() override;
  void CastToSink(const std::string& sink_id) override;
  void StopCasting(const std::string& route_id) override;
  void FreezeRoute(const std::string& route_id) override;
  void UnfreezeRoute(const std::string& route_id) override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  bool IsAccessCodeCastFreezeUiEnabled();

  void StopObservingMirroringMediaControllerHosts();

  void UpdateDevices();

#if !defined(OFFICIAL_BUILD)
  // Adds fake Cast devices for manual testing of the UI (for example, in the
  // linux-chromeos emulator).
  void AddFakeCastDevices();
#endif

  // |device_cache_| stores the current source/route status that we query from.
  // This will return null until the media router is initialized.
  CastDeviceCache* device_cache();
  std::unique_ptr<CastDeviceCache> device_cache_;

  // The list of available devices in a format more palatable for consumption by
  // Ash UI.
  std::vector<ash::SinkAndRoute> devices_;

  base::ObserverList<CastConfigController::Observer> observers_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CAST_CONFIG_CAST_CONFIG_CONTROLLER_MEDIA_ROUTER_H_
