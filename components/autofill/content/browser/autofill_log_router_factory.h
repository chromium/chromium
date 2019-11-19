// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_LOG_ROUTER_FACTORY_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_LOG_ROUTER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace autofill {

class LogRouter;

// BrowserContextKeyedServiceFactory for a LogRouter for autofill internals. It
// does not override BrowserContextKeyedServiceFactory::GetBrowserContextToUse,
// which means that no service is returned in Incognito.
class AutofillLogRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LogRouter* GetForBrowserContext(content::BrowserContext* context);

  static AutofillLogRouterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AutofillLogRouterFactory>;

  AutofillLogRouterFactory();
  ~AutofillLogRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AutofillLogRouterFactory);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_LOG_ROUTER_FACTORY_H_
