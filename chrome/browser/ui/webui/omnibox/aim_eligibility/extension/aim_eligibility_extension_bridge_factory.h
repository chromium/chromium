// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class AimEligibilityExtensionBridge;

class AimEligibilityExtensionBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  static AimEligibilityExtensionBridge* GetForProfile(Profile* profile);
  static AimEligibilityExtensionBridgeFactory* GetInstance();

 private:
  friend base::NoDestructor<AimEligibilityExtensionBridgeFactory>;

  AimEligibilityExtensionBridgeFactory();
  ~AimEligibilityExtensionBridgeFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_EXTENSION_AIM_ELIGIBILITY_EXTENSION_BRIDGE_FACTORY_H_
