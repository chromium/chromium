// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_

#include <memory>

#include "ash/webui/projector_app/projector_app_client.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"
#include "chrome/browser/ui/ash/projector/screencast_manager.h"

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
  const ash::PendingScreencastContainerSet& GetPendingScreencasts()
      const override;
  bool ShouldDownloadSoda() const override;
  void InstallSoda() override;
  void OnSodaInstallProgress(int combined_progress) override;
  void OnSodaInstallError() override;
  void OnSodaInstalled() override;
  void OpenFeedbackDialog() const override;
  void GetVideo(
      const std::string& video_file_id,
      const std::optional<std::string>& resource_key,
      ash::ProjectorAppClient::OnGetVideoCallback callback) const override;
  void NotifyAppUIActive(bool active) override;
  void ToggleFileSyncingNotificationForPaths(
      const std::vector<base::FilePath>& screencast_paths,
      bool suppress) override;
  void HandleAccountReauth(const std::string& email) override;

  PendingScreencastManager* get_pending_screencast_manager_for_test() {
    return &pending_screencast_manager_;
  }

 private:
  void NotifyScreencastsPendingStatusChanged(
      const ash::PendingScreencastContainerSet& pending_screencast_containers);

  base::ObserverList<Observer> observers_;

  // TODO(b/239098953): This should be owned by `screencast_manager_`;
  PendingScreencastManager pending_screencast_manager_;

  ash::ScreencastManager screencast_manager_;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_APP_CLIENT_IMPL_H_
