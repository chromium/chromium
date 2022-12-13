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

}  // namespace apps
