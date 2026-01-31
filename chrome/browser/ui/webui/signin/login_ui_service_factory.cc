// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

LoginUIServiceFactory::LoginUIServiceFactory()
    : ProfileKeyedServiceFactory(
          "LoginUIServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

LoginUIServiceFactory::~LoginUIServiceFactory() = default;

// static
LoginUIService* LoginUIServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LoginUIService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LoginUIServiceFactory* LoginUIServiceFactory::GetInstance() {
  static base::NoDestructor<LoginUIServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
LoginUIServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<LoginUIService>();
}

bool LoginUIServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
