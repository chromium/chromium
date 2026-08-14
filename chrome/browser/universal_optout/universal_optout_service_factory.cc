// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/universal_optout/universal_optout_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/universal_optout/features.h"
#include "components/universal_optout/universal_optout_service.h"
#include "components/variations/service/variations_service.h"

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
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UniversalOptOutServiceFactory::~UniversalOptOutServiceFactory() = default;

std::unique_ptr<KeyedService>
UniversalOptOutServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kUniversalOptOut)) {
    return nullptr;
  }

  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (!variations_service) {
    CHECK_IS_TEST();
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<UniversalOptOutService>(
      CHECK_DEREF(profile->GetPrefs()), *variations_service,
      CHECK_DEREF(IdentityManagerFactory::GetForProfile(profile)));
}

bool UniversalOptOutServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace universal_optout
