// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_context.h"

TrustSafetySentimentServiceFactory::TrustSafetySentimentServiceFactory()
    : ProfileKeyedServiceFactory(
          "TrustSafetySentimentService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HatsServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

TrustSafetySentimentServiceFactory*
TrustSafetySentimentServiceFactory::GetInstance() {
  static base::NoDestructor<TrustSafetySentimentServiceFactory> instance;
  return instance.get();
}

TrustSafetySentimentService* TrustSafetySentimentServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TrustSafetySentimentService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

KeyedService* TrustSafetySentimentServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord() ||
      (!base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurvey) &&
       !base::FeatureList::IsEnabled(
           features::kTrustSafetySentimentSurveyV2))) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);

  // If there is no HaTS service, or the HaTS service reports the user is not
  // eligible to be surveyed by HaTS, do not create the service. This state is
  // unlikely to change over the life of the profile (e.g. before closing
  // Chrome) and simply not creating the service avoids unnecessary work
  // tracking user interactions.
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service ||
      !hats_service->CanShowAnySurvey(/*user_prompted=*/false)) {
    return nullptr;
  }

  return new TrustSafetySentimentService(profile);
}
