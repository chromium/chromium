// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

TabOrganizationServiceFactory::TabOrganizationServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabOrganizationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  }
}

TabOrganizationServiceFactory::~TabOrganizationServiceFactory() = default;

std::unique_ptr<KeyedService>
TabOrganizationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  return features::IsTabOrganization()
             ? std::make_unique<TabOrganizationService>(context)
             : nullptr;
}

// static
TabOrganizationServiceFactory* TabOrganizationServiceFactory::GetInstance() {
  static base::NoDestructor<TabOrganizationServiceFactory> instance;
  return instance.get();
}

// static
TabOrganizationService* TabOrganizationServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<TabOrganizationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
