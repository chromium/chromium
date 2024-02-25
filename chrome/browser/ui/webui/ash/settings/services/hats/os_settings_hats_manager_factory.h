// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_HATS_OS_SETTINGS_HATS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_HATS_OS_SETTINGS_HATS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash::settings {

class OsSettingsHatsManager;

class OsSettingsHatsManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static OsSettingsHatsManager* GetForProfile(Profile* profile);
  static OsSettingsHatsManagerFactory* GetInstance();

  OsSettingsHatsManagerFactory(const OsSettingsHatsManagerFactory&) = delete;
  OsSettingsHatsManagerFactory& operator=(const OsSettingsHatsManagerFactory&) =
      delete;

  KeyedService* SetTestingFactoryAndUse(content::BrowserContext* context,
                                        TestingFactory testing_factory);

 private:
  friend class base::NoDestructor<OsSettingsHatsManagerFactory>;

  OsSettingsHatsManagerFactory();
  ~OsSettingsHatsManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_HATS_OS_SETTINGS_HATS_MANAGER_FACTORY_H_
