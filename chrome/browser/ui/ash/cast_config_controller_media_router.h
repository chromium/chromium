// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAST_CONFIG_CONTROLLER_MEDIA_ROUTER_H_
#define CHROME_BROWSER_UI_ASH_CAST_CONFIG_CONTROLLER_MEDIA_ROUTER_H_

#include <memory>

#include "ash/public/cpp/cast_config_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace media_router {
class MediaRouter;
}

class CastDeviceCache;

// A class which allows the ash tray to communicate with the media router.
class CastConfigControllerMediaRouter : public ash::CastConfigController,
                                        public content::NotificationObserver {
 public:
  CastConfigControllerMediaRouter();
  ~CastConfigControllerMediaRouter() override;

  static void SetMediaRouterForTest(media_router::MediaRouter* media_router);

 private:
  // CastConfigController:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasSinksAndRoutes() const override;
  bool HasActiveRoute() const override;
  void RequestDeviceRefresh() override;
  const std::vector<ash::SinkAndRoute>& GetSinksAndRoutes() override;
  void CastToSink(const std::string& sink_id) override;
  void StopCasting(const std::string& route_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // |device_cache_| stores the current source/route status that we query from.
  // This will return null until the media router is initialized.
  CastDeviceCache* device_cache();
  std::unique_ptr<CastDeviceCache> device_cache_;

  // The list of available devices in a format more palatable for consumption by
  // Ash UI.
  std::vector<ash::SinkAndRoute> devices_;

  base::ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CastConfigControllerMediaRouter);
};

#endif  // CHROME_BROWSER_UI_ASH_CAST_CONFIG_CONTROLLER_MEDIA_ROUTER_H_
