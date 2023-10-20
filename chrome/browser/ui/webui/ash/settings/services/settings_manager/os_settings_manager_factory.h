// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_SETTINGS_MANAGER_OS_SETTINGS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_SETTINGS_MANAGER_OS_SETTINGS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash::settings {

class OsSettingsManager;

class OsSettingsManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static OsSettingsManager* GetForProfile(Profile* profile);
  static OsSettingsManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<OsSettingsManagerFactory>;

  OsSettingsManagerFactory();
  ~OsSettingsManagerFactory() override;

  OsSettingsManagerFactory(const OsSettingsManagerFactory&) = delete;
  OsSettingsManagerFactory& operator=(const OsSettingsManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_SETTINGS_MANAGER_OS_SETTINGS_MANAGER_FACTORY_H_
