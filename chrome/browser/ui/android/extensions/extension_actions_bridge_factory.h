// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTIONS_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTIONS_BRIDGE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace extensions {

class ExtensionActionsBridge;

class ExtensionActionsBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionActionsBridge* GetForProfile(Profile* profile);

  static ExtensionActionsBridgeFactory* GetInstance();

 private:
  friend base::NoDestructor<ExtensionActionsBridgeFactory>;

  ExtensionActionsBridgeFactory();
  ~ExtensionActionsBridgeFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_ACTIONS_BRIDGE_FACTORY_H_
