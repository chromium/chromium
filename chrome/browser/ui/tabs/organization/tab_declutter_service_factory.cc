// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_service.h"
#include "components/keyed_service/core/keyed_service.h"

TabDeclutterServiceFactory::TabDeclutterServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabDeclutterService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

TabDeclutterServiceFactory::~TabDeclutterServiceFactory() = default;

std::unique_ptr<KeyedService>
TabDeclutterServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  return std::make_unique<TabDeclutterService>();
}

// static
TabDeclutterServiceFactory* TabDeclutterServiceFactory::GetInstance() {
  static base::NoDestructor<TabDeclutterServiceFactory> instance;
  return instance.get();
}

// static
TabDeclutterService* TabDeclutterServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<TabDeclutterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
