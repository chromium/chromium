// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_

#include "base/observer_list.h"
#include "chromeos/components/projector_app/projector_app_client.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Implements the interface for Projector App.
class ProjectorAppClientImpl : public chromeos::ProjectorAppClient {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ProjectorAppClientImpl();
  ProjectorAppClientImpl(const ProjectorAppClientImpl&) = delete;
  ProjectorAppClientImpl& operator=(const ProjectorAppClientImpl&) = delete;
  ~ProjectorAppClientImpl() override;

  // chromeos::ProjectorAppClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  signin::IdentityManager* GetIdentityManager() override;
  network::mojom::URLLoaderFactory* GetUrlLoaderFactory() override;
  void OnNewScreencastPreconditionChanged(bool can_start) override;

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
