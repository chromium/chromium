// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"

LoginUIServiceFactory::LoginUIServiceFactory()
    : ProfileKeyedServiceFactory(
          "LoginUIServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(UnifiedConsentServiceFactory::GetInstance());
}

LoginUIServiceFactory::~LoginUIServiceFactory() {}

// static
LoginUIService* LoginUIServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LoginUIService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LoginUIServiceFactory* LoginUIServiceFactory::GetInstance() {
  return base::Singleton<LoginUIServiceFactory>::get();
}

KeyedService* LoginUIServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new LoginUIService(static_cast<Profile*>(profile));
}

bool LoginUIServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
