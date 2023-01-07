// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_SERVICE_ROUTER_FACTORY_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_SERVICE_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace screen_ai {

class ScreenAIServiceRouter;

// Factory to get or create an instance of ScreenAIServiceRouter for a
// BrowserContext.
class ScreenAIServiceRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static screen_ai::ScreenAIServiceRouter* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<ScreenAIServiceRouterFactory>;
  static ScreenAIServiceRouterFactory* GetInstance();

  ScreenAIServiceRouterFactory();
  ~ScreenAIServiceRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_SERVICE_ROUTER_FACTORY_H_
