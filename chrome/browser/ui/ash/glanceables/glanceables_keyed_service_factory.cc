// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"
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
  DependsOn(IdentityManagerFactory::GetInstance());
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
  return std::make_unique<GlanceablesKeyedService>(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
