// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_LENS_LENS_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/lens/lens_keyed_service.h"

class Profile;

class LensKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  LensKeyedServiceFactory(const LensKeyedServiceFactory&) = delete;
  LensKeyedServiceFactory& operator=(const LensKeyedServiceFactory&) = delete;

  static LensKeyedService* GetForProfile(Profile* profile,
                                         bool create_if_necessary);

  static LensKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<LensKeyedServiceFactory>;

  LensKeyedServiceFactory();
  ~LensKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_KEYED_SERVICE_FACTORY_H_
