// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/web_applications/components/web_app_provider_base_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppProvider;

// Singleton that owns all WebAppProviderFactories and associates them with
// Profile.
class WebAppProviderFactory : public WebAppProviderBaseFactory {
 public:
  static WebAppProvider* GetForProfile(Profile* profile);

  static WebAppProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebAppProviderFactory>;

  WebAppProviderFactory();
  ~WebAppProviderFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebAppProviderFactory);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_FACTORY_H_
