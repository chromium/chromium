// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {
namespace settings {

class OsSettingsManager;

class OsSettingsManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OsSettingsManager* GetForProfile(Profile* profile);
  static OsSettingsManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<OsSettingsManagerFactory>;

  OsSettingsManagerFactory();
  ~OsSettingsManagerFactory() override;

  OsSettingsManagerFactory(const OsSettingsManagerFactory&) = delete;
  OsSettingsManagerFactory& operator=(const OsSettingsManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_MANAGER_FACTORY_H_
