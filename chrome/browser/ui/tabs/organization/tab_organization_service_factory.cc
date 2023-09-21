// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "components/keyed_service/core/keyed_service.h"

TabOrganizationServiceFactory::TabOrganizationServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabOrganizationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

TabOrganizationServiceFactory::~TabOrganizationServiceFactory() = default;

std::unique_ptr<KeyedService>
TabOrganizationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  return std::make_unique<TabOrganizationService>();
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
