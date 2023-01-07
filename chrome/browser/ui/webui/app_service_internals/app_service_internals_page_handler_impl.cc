// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_page_handler_impl.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

AppServiceInternalsPageHandlerImpl::AppServiceInternalsPageHandlerImpl(
    Profile* profile,
    mojo::PendingReceiver<
        mojom::app_service_internals::AppServiceInternalsPageHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {}

AppServiceInternalsPageHandlerImpl::~AppServiceInternalsPageHandlerImpl() =
    default;

void AppServiceInternalsPageHandlerImpl::GetApps(GetAppsCallback callback) {
  DCHECK(profile_);

  std::vector<mojom::app_service_internals::AppInfoPtr> result;

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);

  proxy->AppRegistryCache().ForEachApp(
      [&result](const apps::AppUpdate& update) {
        std::stringstream debug_info;
        debug_info << update;

        result.emplace_back(absl::in_place, update.AppId(), update.Name(),
                            debug_info.str());
      });

  std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) { return a->name < b->name; });
  std::move(callback).Run(std::move(result));
}

void AppServiceInternalsPageHandlerImpl::GetPreferredApps(
    GetPreferredAppsCallback callback) {
  DCHECK(profile_);

  std::vector<mojom::app_service_internals::PreferredAppInfoPtr> result;

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);

  base::flat_map<std::string, std::stringstream> debug_info_map;

  for (const auto& preferred_app : proxy->PreferredAppsList().GetReference()) {
    debug_info_map[preferred_app->app_id]
        << preferred_app->intent_filter->ToString() << std::endl;
  }

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
    result.push_back(std::move(ptr));
  }

  std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) { return a->name < b->name; });
  std::move(callback).Run(std::move(result));
}
