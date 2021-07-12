// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_page_handler_impl.h"

#include <sstream>
#include <utility>
#include <vector>

#include "base/template_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/app_update.h"

AppServiceInternalsPageHandlerImpl::AppServiceInternalsPageHandlerImpl(
    Profile* profile)
    : profile_(profile) {}

AppServiceInternalsPageHandlerImpl::~AppServiceInternalsPageHandlerImpl() =
    default;

void AppServiceInternalsPageHandlerImpl::GetApps(GetAppsCallback callback) {
  DCHECK(profile_);

  std::vector<mojom::app_service_internals::AppInfoPtr> result;

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (!proxy) {
    std::move(callback).Run(std::move(result));
    return;
  }

  proxy->AppRegistryCache().ForEachApp(
      [&result](const apps::AppUpdate& update) {
        std::stringstream debug_info;
        debug_info << update;

        result.emplace_back(base::in_place, update.AppId(), update.Name(),
                            debug_info.str());
      });

  std::move(callback).Run(std::move(result));
}
