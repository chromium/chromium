// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_

#include "base/macros.h"
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

 private:
  friend struct base::DefaultSingletonTraits<PasswordManagerLogRouterFactory>;

  PasswordManagerLogRouterFactory();
  ~PasswordManagerLogRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerLogRouterFactory);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_MANAGER_LOG_ROUTER_FACTORY_H_
