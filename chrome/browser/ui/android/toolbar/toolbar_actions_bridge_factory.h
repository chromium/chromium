// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTIONS_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTIONS_BRIDGE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ToolbarActionsBridge;

class ToolbarActionsBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  static ToolbarActionsBridge* GetForProfile(Profile* profile);

  static ToolbarActionsBridgeFactory* GetInstance();

 private:
  friend base::NoDestructor<ToolbarActionsBridgeFactory>;

  ToolbarActionsBridgeFactory();
  ~ToolbarActionsBridgeFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_ACTIONS_BRIDGE_FACTORY_H_
