// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_resource_cache_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

// static
NTPResourceCache* AppResourceCacheFactory::GetForProfile(Profile* profile) {
  return static_cast<NTPResourceCache*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppResourceCacheFactory* AppResourceCacheFactory::GetInstance() {
  static base::NoDestructor<AppResourceCacheFactory> instance;
  return instance.get();
}

AppResourceCacheFactory::AppResourceCacheFactory()
    : ProfileKeyedServiceFactory(
          "AppResourceCache",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

AppResourceCacheFactory::~AppResourceCacheFactory() = default;

std::unique_ptr<KeyedService>
AppResourceCacheFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<NTPResourceCache>(static_cast<Profile*>(profile));
}
