// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/everywhere_omnibox_service_factory.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/everywhere_omnibox_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

// static
EverywhereOmniboxServiceFactory*
EverywhereOmniboxServiceFactory::GetInstance() {
  static base::NoDestructor<EverywhereOmniboxServiceFactory> instance;
  return instance.get();
}

// static
EverywhereOmniboxService* EverywhereOmniboxServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<EverywhereOmniboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

EverywhereOmniboxServiceFactory::EverywhereOmniboxServiceFactory()
    : ProfileKeyedServiceFactory(
          "EverywhereOmniboxService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

EverywhereOmniboxServiceFactory::~EverywhereOmniboxServiceFactory() = default;

std::unique_ptr<KeyedService>
EverywhereOmniboxServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(omnibox::kEverywhereOmnibox)) {
    return nullptr;
  }
  return std::make_unique<EverywhereOmniboxService>(
      Profile::FromBrowserContext(context));
}

bool EverywhereOmniboxServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(omnibox::kEverywhereOmnibox);
}
