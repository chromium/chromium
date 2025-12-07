// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"

// static
SafetyHubHatsServiceFactory* SafetyHubHatsServiceFactory::GetInstance() {
  static base::NoDestructor<SafetyHubHatsServiceFactory> instance;
  return instance.get();
}

// static
SafetyHubHatsService* SafetyHubHatsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SafetyHubHatsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SafetyHubHatsServiceFactory::SafetyHubHatsServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafetyHubHatsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(TrustSafetySentimentServiceFactory::GetInstance());
  DependsOn(HatsServiceFactory::GetInstance());
}

SafetyHubHatsServiceFactory::~SafetyHubHatsServiceFactory() = default;

std::unique_ptr<KeyedService>
SafetyHubHatsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  TrustSafetySentimentService* tss_service =
      TrustSafetySentimentServiceFactory::GetForProfile(profile);
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  return std::make_unique<SafetyHubHatsService>(tss_service, hats_service,
                                                *profile);
}
