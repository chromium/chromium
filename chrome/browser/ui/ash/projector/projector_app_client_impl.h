// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_

#include <memory>

#include "ash/webui/projector_app/projector_app_client.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"
#include "chrome/browser/ui/ash/projector/projector_soda_installation_controller.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Implements the interface for Projector App.
class ProjectorAppClientImpl : public ash::ProjectorAppClient {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ProjectorAppClientImpl();
  ProjectorAppClientImpl(const ProjectorAppClientImpl&) = delete;
  ProjectorAppClientImpl& operator=(const ProjectorAppClientImpl&) = delete;
  ~ProjectorAppClientImpl() override;

  // ash::ProjectorAppClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  signin::IdentityManager* GetIdentityManager() override;
  network::mojom::URLLoaderFactory* GetUrlLoaderFactory() override;
  void OnNewScreencastPreconditionChanged(
      const ash::NewScreencastPrecondition& precondition) override;
  const ash::PendingScreencastSet& GetPendingScreencasts() const override;
  bool ShouldDownloadSoda() override;
  void InstallSoda() override;
  void OnSodaInstallProgress(int combined_progress) override;
  void OnSodaInstallError() override;
  void OnSodaInstalled() override;
  void OpenFeedbackDialog() override;

 private:
  void NotifyScreencastsPendingStatusChanged(
      const ash::PendingScreencastSet& pending_screencast);

  base::ObserverList<Observer> observers_;

  std::unique_ptr<ProjectorSodaInstallationController>
      soda_installation_controller_;
  PendingSreencastManager pending_screencast_manager_;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
