// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/link_capturing_pref_migration.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace web_app {

LinkCapturingPrefMigration::LinkCapturingPrefMigration(Profile& profile)
    : profile_(profile) {
  scoped_observation_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(&*profile_)
           ->AppRegistryCache());
}

LinkCapturingPrefMigration::~LinkCapturingPrefMigration() = default;

void LinkCapturingPrefMigration::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppType() != apps::AppType::kWeb)
    return;

  if (apps_util::IsInstalled(update.PriorReadiness()))
    return;

  if (!apps_util::IsInstalled(update.Readiness()))
    return;

  // The DLC API was never used by system web apps.
  const WebAppProvider* provider = WebAppProvider::GetForWebApps(&*profile_);
  if (!provider)
    return;

  const WebApp* web_app =
      provider->registrar_unsafe().GetAppById(update.AppId());
  if (!web_app)
    return;

  switch (web_app->capture_links()) {
    case blink::mojom::CaptureLinks::kUndefined:
    case blink::mojom::CaptureLinks::kNone:
      return;

    case blink::mojom::CaptureLinks::kNewClient:
    case blink::mojom::CaptureLinks::kExistingClientNavigate:
      apps::AppServiceProxyFactory::GetForProfile(&*profile_)
          ->SetSupportedLinksPreference(update.AppId());
      break;
  }
}

void LinkCapturingPrefMigration::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  scoped_observation_.Reset();
}

}  // namespace web_app
