// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_

#include <string>

#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// A publisher parent class (in the App Service sense) for all app publishers.
// This class has NOTIMPLEMENTED() implementations of mandatory methods from the
// apps::mojom::Publisher class to simplify the process of adding a new
// publisher.
//
// See components/services/app_service/README.md.
class PublisherBase : public apps::mojom::Publisher {
 public:
  PublisherBase();
  ~PublisherBase() override;

  PublisherBase(const PublisherBase&) = delete;
  PublisherBase& operator=(const PublisherBase&) = delete;

  static apps::mojom::AppPtr MakeApp(apps::mojom::AppType app_type,
                                     std::string app_id,
                                     apps::mojom::Readiness readiness,
                                     const std::string& name,
                                     apps::mojom::InstallReason install_reason);

  void FlushMojoCallsForTesting();

 protected:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service,
                  apps::mojom::AppType app_type);

  // Publish |app| to all subscribers in |subscribers|. Should be called
  // whenever the app represented by |app| undergoes some state change to inform
  // subscribers of the change.
  void Publish(apps::mojom::AppPtr app,
               const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers);

  // Modifies CapabilityAccess to all subscribers in |subscribers|.
  void ModifyCapabilityAccess(
      const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers,
      const std::string& app_id,
      absl::optional<bool> accessing_camera,
      absl::optional<bool> accessing_microphone);

  mojo::Receiver<apps::mojom::Publisher>& receiver() { return receiver_; }

 private:
  // apps::mojom::Publisher overrides.
  // DEPRECATED. Prefer passing the files in an Intent through
  // LaunchAppWithIntent.
  // TODO(crbug.com/1264164): Remove this method.
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info,
                           LaunchAppWithIntentCallback callback) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void OnPreferredAppSet(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter,
      apps::mojom::IntentPtr intent,
      apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) override;
  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override;
  void SetResizeLocked(const std::string& app_id,
                       apps::mojom::OptionalBool locked) override;
  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode) override;
  void SetRunOnOsLoginMode(
      const std::string& app_id,
      apps::mojom::RunOnOsLoginMode run_on_os_login_mode) override;

  mojo::Receiver<apps::mojom::Publisher> receiver_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PUBLISHER_BASE_H_
