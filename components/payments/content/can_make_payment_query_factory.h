// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_CAN_MAKE_PAYMENT_QUERY_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_CAN_MAKE_PAYMENT_QUERY_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace payments {

class CanMakePaymentQuery;

// Ensures that there's only one instance of CanMakePaymentQuery per browser
// context.
class CanMakePaymentQueryFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CanMakePaymentQueryFactory* GetInstance();
  CanMakePaymentQuery* GetForContext(content::BrowserContext* context);

  CanMakePaymentQueryFactory(const CanMakePaymentQueryFactory&) = delete;
  CanMakePaymentQueryFactory& operator=(const CanMakePaymentQueryFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<CanMakePaymentQueryFactory>;

  CanMakePaymentQueryFactory();
  ~CanMakePaymentQueryFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_CAN_MAKE_PAYMENT_QUERY_FACTORY_H_
