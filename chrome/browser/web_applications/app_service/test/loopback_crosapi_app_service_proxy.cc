// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class Profile;

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace web_app {

LoopbackCrosapiAppServiceProxy::LoopbackCrosapiAppServiceProxy(Profile* profile)
    : app_service_(
          apps::AppServiceProxyFactory::GetForProfile(profile)->GetWeakPtr()) {
  DCHECK(app_service_);
  app_service_->SetCrosapiAppServiceProxyForTesting(this);
  app_service_->LacrosWebAppsControllerForTesting()->SetPublisherForTesting(
      this);
  app_service_->AsAppServiceSubscriberForTesting()->InitializePreferredApps({});
}

LoopbackCrosapiAppServiceProxy::~LoopbackCrosapiAppServiceProxy() {
  DCHECK(app_service_);
  app_service_->SetCrosapiAppServiceProxyForTesting(nullptr);
  app_service_->LacrosWebAppsControllerForTesting()->SetPublisherForTesting(
      nullptr);
}

void LoopbackCrosapiAppServiceProxy::RemoveSupportedLinksPreference(
    const std::string& app_id) {
  PostTask(base::BindOnce(
      &LoopbackCrosapiAppServiceProxy::RemoveSupportedLinksPreferenceInternal,
      weak_ptr_factory_.GetWeakPtr(), app_id));
}

void LoopbackCrosapiAppServiceProxy::RegisterAppServiceSubscriber(
    mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}
void LoopbackCrosapiAppServiceProxy::Launch(
    crosapi::mojom::LaunchParamsPtr launch_params) {
  app_service_->LacrosWebAppsControllerForTesting()->Launch(
      std::move(launch_params), base::DoNothing());
}

void LoopbackCrosapiAppServiceProxy::LaunchWithResult(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchWithResultCallback callback) {
  app_service_->LacrosWebAppsControllerForTesting()->Launch(
      std::move(launch_params), std::move(callback));
}

void LoopbackCrosapiAppServiceProxy::LoadIcon(const std::string& app_id,
                                              apps::IconKeyPtr icon_key,
                                              apps::IconType icon_type,
                                              int32_t size_hint_in_dip,
                                              apps::LoadIconCallback callback) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}
void LoopbackCrosapiAppServiceProxy::AddPreferredAppDeprecated(
    const std::string& app_id,
    crosapi::mojom::IntentPtr intent) {
  NOTIMPLEMENTED();
}
void LoopbackCrosapiAppServiceProxy::ShowAppManagementPage(
    const std::string& app_id) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}

void LoopbackCrosapiAppServiceProxy::SetSupportedLinksPreference(
    const std::string& app_id) {
  PostTask(base::BindOnce(
      &LoopbackCrosapiAppServiceProxy::SetSupportedLinksPreferenceInternal,
      weak_ptr_factory_.GetWeakPtr(), app_id));
}

void LoopbackCrosapiAppServiceProxy::UninstallSilently(
    const std::string& app_id,
    apps::UninstallSource uninstall_source) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}

void LoopbackCrosapiAppServiceProxy::InstallAppWithFallback(
    crosapi::mojom::InstallAppParamsPtr params,
    InstallAppWithFallbackCallback callback) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}

void LoopbackCrosapiAppServiceProxy::OnApps(std::vector<apps::AppPtr> deltas) {
  PostTask(base::BindOnce(&LoopbackCrosapiAppServiceProxy::OnAppsInternal,
                          weak_ptr_factory_.GetWeakPtr(), std::move(deltas)));
}

void LoopbackCrosapiAppServiceProxy::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}

void LoopbackCrosapiAppServiceProxy::OnCapabilityAccesses(
    std::vector<apps::CapabilityAccessPtr> deltas) {
  // Implement this if needed.
  NOTIMPLEMENTED();
}

//// Internal methods for enabling async calls. ////

void LoopbackCrosapiAppServiceProxy::PostTask(base::OnceClosure closure) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

void LoopbackCrosapiAppServiceProxy::RemoveSupportedLinksPreferenceInternal(
    const std::string& app_id) {
  DCHECK(app_service_);

  if (!app_service_->PreferredAppsList().IsPreferredAppForSupportedLinks(
          app_id)) {
    return;
  }

  apps::IntentFilters filters;
  app_service_->AppRegistryCache().ForOneApp(
      app_id, [&app_id, &filters](const apps::AppUpdate& app) {
        for (auto& filter : app.IntentFilters()) {
          if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
            filters.push_back(std::move(filter));
          }
        }
      });

  auto changes = std::make_unique<apps::PreferredAppChanges>();
  changes->removed_filters[app_id] = std::move(filters);

  app_service_->AsAppServiceSubscriberForTesting()->OnPreferredAppsChanged(
      std::move(changes));
}

void LoopbackCrosapiAppServiceProxy::SetSupportedLinksPreferenceInternal(
    const std::string& app_id) {
  DCHECK(app_service_);

  if (app_service_->PreferredAppsList().IsPreferredAppForSupportedLinks(
          app_id)) {
    return;
  }

  apps::IntentFilters filters;
  app_service_->AppRegistryCache().ForOneApp(
      app_id, [&app_id, &filters](const apps::AppUpdate& app) {
        for (auto& filter : app.IntentFilters()) {
          if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
            filters.push_back(std::move(filter));
          }
        }
      });

  auto changes = std::make_unique<apps::PreferredAppChanges>();
  changes->added_filters[app_id] = std::move(filters);

  app_service_->AsAppServiceSubscriberForTesting()->OnPreferredAppsChanged(
      std::move(changes));
}

void LoopbackCrosapiAppServiceProxy::OnAppsInternal(
    std::vector<apps::AppPtr> deltas) {
  if (!app_service_) {
    return;
  }
  app_service_->AsAppServiceSubscriberForTesting()->OnApps(
      std::move(deltas), apps::AppType::kWeb,
      /*should_notify_initialized=*/true);
}

}  // namespace web_app
