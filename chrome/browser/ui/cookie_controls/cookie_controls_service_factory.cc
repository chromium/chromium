// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"

// static
CookieControlsService* CookieControlsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CookieControlsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CookieControlsServiceFactory* CookieControlsServiceFactory::GetInstance() {
  static base::NoDestructor<CookieControlsServiceFactory> instance;
  return instance.get();
}

// static
KeyedService* CookieControlsServiceFactory::BuildInstanceFor(Profile* profile) {
  return new CookieControlsService(profile);
}

CookieControlsServiceFactory::CookieControlsServiceFactory()
    : ProfileKeyedServiceFactory(
          "CookieControlsService",
          // The incognito profile has its own CookieSettings. Therefore, it
          // should get its own CookieControlsService.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

CookieControlsServiceFactory::~CookieControlsServiceFactory() = default;

KeyedService* CookieControlsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(Profile::FromBrowserContext(profile));
}
