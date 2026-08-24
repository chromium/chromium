// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/user_education/user_education_service_factory.h"

// static
OmniboxEverywhereServiceFactory*
OmniboxEverywhereServiceFactory::GetInstance() {
  static base::NoDestructor<OmniboxEverywhereServiceFactory> instance;
  return instance.get();
}

// static
OmniboxEverywhereService* OmniboxEverywhereServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OmniboxEverywhereService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

OmniboxEverywhereServiceFactory::OmniboxEverywhereServiceFactory()
    : ProfileKeyedServiceFactory(
          "OmniboxEverywhereService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(UserEducationServiceFactory::GetInstance());
}

OmniboxEverywhereServiceFactory::~OmniboxEverywhereServiceFactory() = default;

std::unique_ptr<KeyedService>
OmniboxEverywhereServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere)) {
    return nullptr;
  }
  return std::make_unique<OmniboxEverywhereService>(
      Profile::FromBrowserContext(context));
#else
  return nullptr;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}

bool OmniboxEverywhereServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere);
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}
