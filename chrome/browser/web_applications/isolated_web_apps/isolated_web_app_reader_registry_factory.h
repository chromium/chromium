// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

// Singleton that owns all `IsolatedWebAppReaderRegistry`s and associates them
// with a `Profile`.
//
// We don't actually use the `Profile` inside
// `IsolatedWebAppReaderRegistry`. Having a separate
// `IsolatedWebAppReaderRegistry` per `Profile` is purely a security measure,
// which makes sure that the integrity of an Isolated Web App is verified on a
// per-profile basis.
class IsolatedWebAppReaderRegistryFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  IsolatedWebAppReaderRegistryFactory(
      const IsolatedWebAppReaderRegistryFactory&) = delete;
  IsolatedWebAppReaderRegistryFactory& operator=(
      const IsolatedWebAppReaderRegistryFactory&) = delete;

  static IsolatedWebAppReaderRegistryFactory* GetInstance();

  static IsolatedWebAppReaderRegistry* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<IsolatedWebAppReaderRegistryFactory>;

  IsolatedWebAppReaderRegistryFactory();
  ~IsolatedWebAppReaderRegistryFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_FACTORY_H_
