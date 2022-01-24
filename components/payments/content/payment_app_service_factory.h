// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_SERVICE_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {

class PaymentAppService;

// Maps payment app service to browser context.
class PaymentAppServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PaymentAppService* GetForContext(content::BrowserContext* context);

  PaymentAppServiceFactory(const PaymentAppServiceFactory&) = delete;
  PaymentAppServiceFactory& operator=(const PaymentAppServiceFactory&) = delete;

  // Used only in tests to set the |service| that is going to be returned from
  // |GetForContext()|, even if |context| is null.
  static void SetForTesting(std::unique_ptr<PaymentAppService> service);

 private:
  friend struct base::DefaultSingletonTraits<PaymentAppServiceFactory>;

  static PaymentAppServiceFactory* GetInstance();

  PaymentAppServiceFactory();
  ~PaymentAppServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  std::unique_ptr<PaymentAppService> service_for_testing_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_APP_SERVICE_FACTORY_H_
