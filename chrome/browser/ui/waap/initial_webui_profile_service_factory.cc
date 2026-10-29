// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service.h"
#include "chrome/common/chrome_features.h"

// static
InitialWebUIProfileService* InitialWebUIProfileServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<InitialWebUIProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InitialWebUIProfileServiceFactory*
InitialWebUIProfileServiceFactory::GetInstance() {
  static base::NoDestructor<InitialWebUIProfileServiceFactory> instance;
  return instance.get();
}

InitialWebUIProfileServiceFactory::InitialWebUIProfileServiceFactory()
    : ProfileKeyedServiceFactory(
          "InitialWebUIProfileService",
          ProfileSelections::Builder()
              // Prewarm only for regular profiles, not incognito.
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

InitialWebUIProfileServiceFactory::~InitialWebUIProfileServiceFactory() =
    default;

std::unique_ptr<KeyedService>
InitialWebUIProfileServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // The service prewarms the toolbar WebContents in each regular profile when
  // the profile is initialized. With kTopChromeWebUISharedRenderer, the toolbar
  // WebContents is created in TopChromeWebUIProfile instead, so the service
  // isn't needed.
  if (base::FeatureList::IsEnabled(features::kTopChromeWebUISharedRenderer)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<InitialWebUIProfileService>(profile);
}

bool InitialWebUIProfileServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Eagerly instantiate this service so prewarming starts as soon as the
  // profile is ready.
  return true;
}
