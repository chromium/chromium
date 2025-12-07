// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/policy/policy_blocklist_service/ash_policy_blocklist_service_factory.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace {

constexpr char kFeatureStatusHistogram[] =
    "Ash.Glanceables.TimeManagement.FeatureStatus";

// Indicates whether, and why time management glanceables are enabled.
// Used as an enum in histograms, so the assigned values should not change.
// Note this should be kept in sync with
// `TimeManagementGlanceablesFeatureStatus` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class GlanceablesStatus {
  kDisabled = 0,
  kDEPRECATED_EnabledForTrustedTesters = 1,
  kDEPRECATED_EnabledByV2Flag = 2,
  kDEPRECATED_EnabledByPrefBypass = 3,
  kEnabledForFullLaunch = 4,
  kMaxValue = kEnabledForFullLaunch
};

bool ShouldCreateServiceInstance() {
  if (features::AreAnyGlanceablesTimeManagementViewsEnabled()) {
    base::UmaHistogramEnumeration(kFeatureStatusHistogram,
                                  GlanceablesStatus::kEnabledForFullLaunch);
    return true;
  }
  base::UmaHistogramEnumeration(kFeatureStatusHistogram,
                                GlanceablesStatus::kDisabled);
  return false;
}

}  // namespace

// static
GlanceablesKeyedServiceFactory* GlanceablesKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<GlanceablesKeyedServiceFactory> factory;
  return factory.get();
}

GlanceablesKeyedServiceFactory::GlanceablesKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "GlanceablesKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  // LINT.IfChange(Deps)
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(AshPolicyBlocklistServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  // LINT.ThenChange(//chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h:Deps)
}

GlanceablesKeyedService* GlanceablesKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<GlanceablesKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(
          context, ShouldCreateServiceInstance()));
}

std::unique_ptr<KeyedService>
GlanceablesKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)
          ? apps::AppServiceProxyFactory::GetForProfile(profile)
          : nullptr;
  return std::make_unique<GlanceablesKeyedService>(
      BrowserContextHelper::Get()
          ->GetUserByBrowserContext(profile)
          ->GetAccountId(),
      profile->GetPrefs(), app_service_proxy,
      AshPolicyBlocklistServiceFactory::GetForBrowserContext(profile),
      profile->GetURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace ash
