// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
namespace unified_consent {
class UnifiedConsentService;
}

class UnifiedConsentServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of UnifiedConsentService associated with |profile|
  // (creating one if none exists). Returns nullptr if this profile cannot have
  // a UnifiedConsentService (e.g. sync is disabled for |profile| or
  // |profile| is incognito).
  static unified_consent::UnifiedConsentService* GetForProfile(
      Profile* profile);

  // Returns an instance of the factory singleton.
  static UnifiedConsentServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UnifiedConsentServiceFactory>;

  UnifiedConsentServiceFactory();
  ~UnifiedConsentServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentServiceFactory);
};

#endif  // CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_
