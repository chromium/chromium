// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/diagnostics/app_type_initialized_event.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace web_app {

AppTypeInitializedEvent::AppTypeInitializedEvent(Profile* profile,
                                                 apps::AppType app_type)
    : profile_(profile), app_type_(app_type) {}

AppTypeInitializedEvent::~AppTypeInitializedEvent() = default;

bool AppTypeInitializedEvent::Post(base::OnceClosure callback) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_.get())) {
    return false;
  }

  apps::AppRegistryCache& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile_.get())
          ->AppRegistryCache();
  if (app_registry_cache.IsAppTypeInitialized(app_type_)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return true;
  }

  scoped_observation_.Observe(&app_registry_cache);
  callback_ = std::move(callback);
  return true;
}

void AppTypeInitializedEvent::OnAppTypeInitialized(apps::AppType app_type) {
  if (!callback_ || app_type != app_type_)
    return;
  scoped_observation_.Reset();
  std::move(callback_).Run();
}

void AppTypeInitializedEvent::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  scoped_observation_.Reset();
}

}  // namespace web_app
