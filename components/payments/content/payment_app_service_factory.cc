// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app_service_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/payments/content/payment_app_service.h"

namespace payments {

// static
PaymentAppService* PaymentAppServiceFactory::GetForContext(
    content::BrowserContext* context) {
  auto* instance = GetInstance();
  return instance->service_for_testing_
             ? instance->service_for_testing_.get()
             : static_cast<PaymentAppService*>(
                   instance->GetServiceForBrowserContext(context,
                                                         /*create=*/true));
}

// static
void PaymentAppServiceFactory::SetForTesting(
    std::unique_ptr<PaymentAppService> service) {
  GetInstance()->service_for_testing_ = std::move(service);
}

// static
PaymentAppServiceFactory* PaymentAppServiceFactory::GetInstance() {
  return base::Singleton<PaymentAppServiceFactory>::get();
}

PaymentAppServiceFactory::PaymentAppServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PaymentAppService",
          BrowserContextDependencyManager::GetInstance()) {}

PaymentAppServiceFactory::~PaymentAppServiceFactory() = default;

KeyedService* PaymentAppServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PaymentAppService(context);
}

content::BrowserContext* PaymentAppServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Returns non-null even for Incognito contexts so that a separate instance of
  // a service is created for the Incognito context.
  return context;
}

}  // namespace payments
