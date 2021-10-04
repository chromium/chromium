// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_

#include "base/observer_list.h"
#include "chromeos/components/projector_app/projector_app_client.h"

// Implements the interface for Projector App.
class ProjectorAppClientImpl : public chromeos::ProjectorAppClient {
 public:
  ProjectorAppClientImpl();
  ProjectorAppClientImpl(const ProjectorAppClientImpl&) = delete;
  ProjectorAppClientImpl& operator=(const ProjectorAppClientImpl&) = delete;
  ~ProjectorAppClientImpl() override;

  // chromeos::ProjectorAppClient:
  signin::IdentityManager* GetIdentityManager() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
