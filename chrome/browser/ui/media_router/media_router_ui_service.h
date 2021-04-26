// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_H_

#include <memory>

#include "base/observer_list.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefChangeRegistrar;
class Profile;

namespace media_router {

// Service that owns per-profile Media Router UI objects, such as the controller
// for the Media Router toolbar action.
class MediaRouterUIService : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnServiceDisabled() = 0;
  };

  explicit MediaRouterUIService(Profile* profile);
  // Used by tests to inject an action controller.
  MediaRouterUIService(
      Profile* profile,
      std::unique_ptr<MediaRouterActionController> action_controller);
  ~MediaRouterUIService() override;

  // KeyedService:
  void Shutdown() override;

  static MediaRouterUIService* Get(Profile* profile);

  virtual MediaRouterActionController* action_controller();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class MediaRouterUIBrowserTest;

  void ConfigureService();
  void DisableService();

  Profile* profile_;
  std::unique_ptr<MediaRouterActionController> action_controller_;
  std::unique_ptr<PrefChangeRegistrar> profile_pref_registrar_;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUIService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_H_
