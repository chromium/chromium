// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_error/global_error_service.h"

// static
GlobalErrorService* GlobalErrorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<GlobalErrorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GlobalErrorServiceFactory* GlobalErrorServiceFactory::GetInstance() {
  static base::NoDestructor<GlobalErrorServiceFactory> instance;
  return instance.get();
}

GlobalErrorServiceFactory::GlobalErrorServiceFactory()
    : ProfileKeyedServiceFactory(
          "GlobalErrorService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

GlobalErrorServiceFactory::~GlobalErrorServiceFactory() = default;

KeyedService* GlobalErrorServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new GlobalErrorService();
}
