// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

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
  // For the refill, which watches the profile's browser windows.
  DependsOn(BrowserManagerServiceFactory::GetInstance());
}

InitialWebUIProfileServiceFactory::~InitialWebUIProfileServiceFactory() =
    default;

std::unique_ptr<KeyedService>
InitialWebUIProfileServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // The service prewarms the toolbar WebContents in each regular profile when
  // the profile is initialized. If ShouldUseTopChromeWebUIProfile(), the
  // toolbar WebContents is created in TopChromeWebUIProfile instead, so the
  // service isn't needed unless it prerenders per-profile spares. With
  // IsUnboundPrerenderingEnabled(), one process-wide unbound spare serves every
  // window instead of per-profile spares. It is still needed with
  // kTopChromeWebUISharedRenderer in --single-process mode, where the toolbar
  // stays in the user profile.
  //
  // A prerendering service creates each spare in TopChromeWebUIProfile with
  // this profile set as its user profile before navigation, and only windows
  // of this profile adopt it. At startup, the initial profile's service is
  // built before PreBrowserStart() calls CreateTopChromeWebUIProfile(), so
  // GetTopChromeWebUIProfile() is still null here. This relies on prerendering
  // creating no WebContents when the profile is initialized: the first spare
  // is built after the profile's first window opens.
  if (webui::ShouldUseTopChromeWebUIProfile() &&
      (!InitialWebUIProfileService::IsPrerenderingEnabled() ||
       InitialWebUIProfileService::IsUnboundPrerenderingEnabled())) {
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
