// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_APP_RESOURCE_CACHE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_APP_RESOURCE_CACHE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NTPResourceCache;
class Profile;

// Singleton that owns NTPResourceCaches used by the apps launcher page and
// associates them with Profiles. Listens for the Profile's destruction
// notification and cleans up the associated ThemeService.
class AppResourceCacheFactory : public ProfileKeyedServiceFactory {
 public:
  static NTPResourceCache* GetForProfile(Profile* profile);

  static AppResourceCacheFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppResourceCacheFactory>;

  AppResourceCacheFactory();
  ~AppResourceCacheFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_APP_RESOURCE_CACHE_FACTORY_H_
