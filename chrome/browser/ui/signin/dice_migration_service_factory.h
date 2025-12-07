// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DiceMigrationService;

class DiceMigrationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DiceMigrationService* GetForProfile(Profile* profile);
  static DiceMigrationService* GetForProfileIfExists(Profile* profile);

  // Returns an instance of the DiceMigrationServiceFactory singleton.
  static DiceMigrationServiceFactory* GetInstance();

  DiceMigrationServiceFactory(const DiceMigrationServiceFactory&) = delete;
  DiceMigrationServiceFactory& operator=(const DiceMigrationServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<DiceMigrationServiceFactory>;

  DiceMigrationServiceFactory();
  ~DiceMigrationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_FACTORY_H_
