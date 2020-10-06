// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BrowserContextDependencyManager;
class Profile;

namespace web_app {

class WebAppProviderBase;

// Singleton that associates WebAppProviderBase with Profile.
class WebAppProviderBaseFactory : public BrowserContextKeyedServiceFactory {
 public:
  WebAppProviderBaseFactory(const WebAppProviderBaseFactory&) = delete;
  WebAppProviderBaseFactory& operator=(const WebAppProviderBaseFactory&) =
      delete;

  static WebAppProviderBase* GetForProfile(Profile* profile);

  static WebAppProviderBaseFactory* GetInstance();
  static void SetInstance(WebAppProviderBaseFactory* factory);

 protected:
  WebAppProviderBaseFactory(
      const char* service_name,
      BrowserContextDependencyManager* dependency_manager);
  ~WebAppProviderBaseFactory() override;

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROVIDER_BASE_FACTORY_H_
