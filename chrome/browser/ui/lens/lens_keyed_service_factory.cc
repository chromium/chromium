// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_keyed_service_factory.h"

// static
LensKeyedService* LensKeyedServiceFactory::GetForProfile(
    Profile* profile,
    bool create_if_necessary) {
  return static_cast<LensKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
LensKeyedServiceFactory* LensKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<LensKeyedServiceFactory> instance;
  return instance.get();
}

LensKeyedServiceFactory::LensKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "LensKeyedService",
          ProfileSelections::Builder()
              // Do not keep a separate counter for incognito.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}
std::unique_ptr<KeyedService>
LensKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<LensKeyedService>();
}

LensKeyedServiceFactory::~LensKeyedServiceFactory() = default;

bool LensKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
