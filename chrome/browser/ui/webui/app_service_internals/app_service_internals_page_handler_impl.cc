// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_page_handler_impl.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom-forward.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

std::vector<mojom::app_service_internals::AppInfoPtr> GetApps(
    apps::AppServiceProxy* proxy) {
  std::vector<mojom::app_service_internals::AppInfoPtr> apps;

  proxy->AppRegistryCache().ForEachApp([&apps](const apps::AppUpdate& update) {
    std::stringstream debug_info;
    debug_info << update;

    apps.emplace_back(std::in_place, update.AppId(), update.Name(),
                      debug_info.str());
  });

  base::ranges::sort(apps, std::less<>(),
                     [](const auto& app) { return app->name; });

  return apps;
}

std::vector<mojom::app_service_internals::PreferredAppInfoPtr> GetPreferredApps(
    apps::AppServiceProxy* proxy) {
  base::flat_map<std::string, std::stringstream> debug_info_map;

  for (const auto& preferred_app : proxy->PreferredAppsList().GetReference()) {
    debug_info_map[preferred_app->app_id]
        << preferred_app->intent_filter->ToString() << std::endl;
  }

  std::vector<mojom::app_service_internals::PreferredAppInfoPtr> preferred_apps;
  for (const auto& kv : debug_info_map) {
    auto ptr = mojom::app_service_internals::PreferredAppInfo::New();
    ptr->id = kv.first;

    if (ptr->id == apps_util::kUseBrowserForLink) {
      ptr->name = ptr->id;
    } else {
      proxy->AppRegistryCache().ForOneApp(
          kv.first,
          [&ptr](const apps::AppUpdate& update) { ptr->name = update.Name(); });
    }
    ptr->preferred_filters = kv.second.str();
    preferred_apps.push_back(std::move(ptr));
  }

  base::ranges::sort(preferred_apps, std::less<>(),
                     [](const auto& app) { return app->name; });
  return preferred_apps;
}

std::vector<mojom::app_service_internals::PromiseAppInfoPtr> GetPromiseApps(
    apps::AppServiceProxy* proxy) {
  std::vector<mojom::app_service_internals::PromiseAppInfoPtr> promise_apps;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::features::ArePromiseIconsEnabled() ||
      !proxy->PromiseAppRegistryCache()) {
    return promise_apps;
  }

  for (const auto& promise_app :
       proxy->PromiseAppRegistryCache()->GetAllPromiseApps()) {
    std::stringstream debug_info;
    debug_info << *promise_app;
    promise_apps.emplace_back(std::in_place,
                              promise_app.get()->package_id.ToString(),
                              debug_info.str());
  }

  base::ranges::sort(promise_apps, std::less<>(), [](const auto& promise_app) {
    return promise_app->package_id;
  });

#endif
  return promise_apps;
}

std::vector<mojom::app_service_internals::AppCapabilityInfoPtr>
GetAppCapabilities(apps::AppServiceProxy* proxy) {
  std::vector<mojom::app_service_internals::AppCapabilityInfoPtr>
      app_capabilities;

  proxy->AppCapabilityAccessCache().ForEachApp(
      [proxy,
       &app_capabilities](const apps::CapabilityAccessUpdate& app_capability) {
        std::stringstream debug_info;
        debug_info << app_capability;

        std::string name;
        proxy->AppRegistryCache().ForOneApp(
            app_capability.AppId(), [&name](const apps::AppUpdate& app_update) {
              name = app_update.Name();
            });

        app_capabilities.emplace_back(std::in_place, name, debug_info.str());
      });

  return app_capabilities;
}

}  // namespace

AppServiceInternalsPageHandlerImpl::AppServiceInternalsPageHandlerImpl(
    Profile* profile,
    mojo::PendingReceiver<
        mojom::app_service_internals::AppServiceInternalsPageHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {}

AppServiceInternalsPageHandlerImpl::~AppServiceInternalsPageHandlerImpl() =
    default;

void AppServiceInternalsPageHandlerImpl::GetDebugInfo(
    GetDebugInfoCallback callback) {
  CHECK(profile_);

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_)) {
    std::move(callback).Run(std::move(nullptr));
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  CHECK(proxy);

  mojom::app_service_internals::DebugInfoPtr result =
      mojom::app_service_internals::DebugInfo::New();
  result->app_list = GetApps(proxy);
  result->preferred_app_list = GetPreferredApps(proxy);
  result->promise_app_list = GetPromiseApps(proxy);
  result->app_capability_list = GetAppCapabilities(proxy);

  std::move(callback).Run(std::move(result));
}
