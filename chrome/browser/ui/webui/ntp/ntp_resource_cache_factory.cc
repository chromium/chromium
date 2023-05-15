// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

// static
NTPResourceCache* NTPResourceCacheFactory::GetForProfile(Profile* profile) {
  return static_cast<NTPResourceCache*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NTPResourceCacheFactory* NTPResourceCacheFactory::GetInstance() {
  return base::Singleton<NTPResourceCacheFactory>::get();
}

NTPResourceCacheFactory::NTPResourceCacheFactory()
    : ProfileKeyedServiceFactory(
          "NTPResourceCache",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ThemeServiceFactory::GetInstance());
}

NTPResourceCacheFactory::~NTPResourceCacheFactory() {}

KeyedService* NTPResourceCacheFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new NTPResourceCache(static_cast<Profile*>(profile));
}
