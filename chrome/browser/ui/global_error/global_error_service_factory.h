// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class GlobalErrorService;
class Profile;

// Singleton that owns all GlobalErrorService and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated GlobalErrorService.
class GlobalErrorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  GlobalErrorServiceFactory(const GlobalErrorServiceFactory&) = delete;
  GlobalErrorServiceFactory& operator=(const GlobalErrorServiceFactory&) =
      delete;

  static GlobalErrorService* GetForProfile(Profile* profile);

  static GlobalErrorServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<GlobalErrorServiceFactory>;

  GlobalErrorServiceFactory();
  ~GlobalErrorServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_FACTORY_H_
