// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace autofill {
class LogRouter;
}

namespace content {
class BrowserContext;
}

namespace password_manager {

// BrowserContextKeyedServiceFactory for a LogRouter for password manager
// internals. It does not override
// BrowserContextKeyedServiceFactory::GetBrowserContextToUse,
// which means that no service is returned in Incognito.
class PasswordManagerLogRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static autofill::LogRouter* GetForBrowserContext(
      content::BrowserContext* context);

  static PasswordManagerLogRouterFactory* GetInstance();

  PasswordManagerLogRouterFactory(const PasswordManagerLogRouterFactory&) =
      delete;
  PasswordManagerLogRouterFactory& operator=(
      const PasswordManagerLogRouterFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PasswordManagerLogRouterFactory>;

  PasswordManagerLogRouterFactory();
  ~PasswordManagerLogRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
