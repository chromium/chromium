// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/publisher_base.h"

#include <vector>

#include "base/notreached.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/features.h"

namespace apps {

PublisherBase::PublisherBase() = default;

PublisherBase::~PublisherBase() = default;

// static
apps::mojom::AppPtr PublisherBase::MakeApp(
    apps::mojom::AppType app_type,
    std::string app_id,
    apps::mojom::Readiness readiness,
    const std::string& name,
    apps::mojom::InstallReason install_reason) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = app_type;
  app->app_id = app_id;
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;

  app->last_launch_time = base::Time();
  app->install_time = base::Time();

  app->install_reason = install_reason;
  app->install_source = apps::mojom::InstallSource::kUnknown;

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->paused = apps::mojom::OptionalBool::kFalse;

  return app;
}

void PublisherBase::FlushMojoCallsForTesting() {
  if (receiver_.is_bound()) {
    receiver_.FlushForTesting();
  }
}

void PublisherBase::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    apps::mojom::AppType app_type) {
  if (!base::FeatureList::IsEnabled(kStopMojomAppService)) {
    app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                   app_type);
  }
}

void PublisherBase::Publish(
    apps::mojom::AppPtr app,
    const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers) {
  for (auto& subscriber : subscribers) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps), apps::mojom::AppType::kUnknown,
                       false /* should_notify_initialized */);
  }
}

void PublisherBase::ModifyCapabilityAccess(
    const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers,
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  for (auto& subscriber : subscribers) {
    std::vector<apps::mojom::CapabilityAccessPtr> capability_accesses;
    auto capability_access = apps::mojom::CapabilityAccess::New();
    capability_access->app_id = app_id;

    if (accessing_camera.has_value()) {
      capability_access->camera = accessing_camera.value()
                                      ? apps::mojom::OptionalBool::kTrue
                                      : apps::mojom::OptionalBool::kFalse;
    }

    if (accessing_microphone.has_value()) {
      capability_access->microphone = accessing_microphone.value()
                                          ? apps::mojom::OptionalBool::kTrue
                                          : apps::mojom::OptionalBool::kFalse;
    }

    capability_accesses.push_back(std::move(capability_access));
    subscriber->OnCapabilityAccesses(std::move(capability_accesses));
  }
}

void PublisherBase::LaunchAppWithFiles(const std::string& app_id,
                                       int32_t event_flags,
                                       apps::mojom::LaunchSource launch_source,
                                       apps::mojom::FilePathsPtr file_paths) {
  NOTIMPLEMENTED();
}

void PublisherBase::LaunchAppWithIntent(const std::string& app_id,
                                        int32_t event_flags,
                                        apps::mojom::IntentPtr intent,
                                        apps::mojom::LaunchSource launch_source,
                                        apps::mojom::WindowInfoPtr window_info,
                                        LaunchAppWithIntentCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*success=*/false);
}

void PublisherBase::SetPermission(const std::string& app_id,
                                  apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void PublisherBase::Uninstall(const std::string& app_id,
                              apps::mojom::UninstallSource uninstall_source,
                              bool clear_site_data,
                              bool report_abuse) {
  LOG(ERROR) << "Uninstall failed, could not remove the app with id " << app_id;
}

void PublisherBase::PauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::UnpauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::StopApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::GetMenuModel(const std::string& app_id,
                                 apps::mojom::MenuType menu_type,
                                 int64_t display_id,
                                 GetMenuModelCallback callback) {
  NOTIMPLEMENTED();
}

void PublisherBase::ExecuteContextMenuCommand(const std::string& app_id,
                                              int command_id,
                                              const std::string& shortcut_id,
                                              int64_t display_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PublisherBase::SetResizeLocked(const std::string& app_id,
                                    apps::mojom::OptionalBool locked) {
  NOTIMPLEMENTED();
}

void PublisherBase::SetWindowMode(const std::string& app_id,
                                  apps::mojom::WindowMode window_mode) {
  NOTIMPLEMENTED();
}

void PublisherBase::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
  NOTIMPLEMENTED();
}

}  // namespace apps
