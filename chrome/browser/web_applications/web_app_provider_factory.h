// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppProvider;

// Singleton that owns all WebAppProviderFactories and associates them with
// Profile. Clients of WebAppProvider cannot use this class to obtain
// WebAppProvider instances, instead they should call WebAppProvider static
// methods.
class WebAppProviderFactory : public BrowserContextKeyedServiceFactory {
 public:
  WebAppProviderFactory(const WebAppProviderFactory&) = delete;
  WebAppProviderFactory& operator=(const WebAppProviderFactory&) = delete;

  static WebAppProviderFactory* GetInstance();

  static bool IsServiceCreatedForProfile(Profile* profile);

 private:
  friend base::NoDestructor<WebAppProviderFactory>;
  friend class WebAppProvider;

  WebAppProviderFactory();
  ~WebAppProviderFactory() override;

  // Called by WebAppProvider static methods.
  static WebAppProvider* GetForProfile(Profile* profile);

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_FACTORY_H_
