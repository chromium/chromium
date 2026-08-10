// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/universal_optout/universal_optout_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/universal_optout/features.h"
#include "components/universal_optout/universal_optout_service.h"

namespace universal_optout {

// static
UniversalOptOutServiceFactory* UniversalOptOutServiceFactory::GetInstance() {
  static base::NoDestructor<UniversalOptOutServiceFactory> instance;
  return instance.get();
}

// static
UniversalOptOutService* UniversalOptOutServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UniversalOptOutService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UniversalOptOutServiceFactory::UniversalOptOutServiceFactory()
    : ProfileKeyedServiceFactory(
          "UniversalOptOutService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

UniversalOptOutServiceFactory::~UniversalOptOutServiceFactory() = default;

std::unique_ptr<KeyedService>
UniversalOptOutServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kUniversalOptOut)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<UniversalOptOutService>(profile->GetPrefs());
}

bool UniversalOptOutServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace universal_optout
