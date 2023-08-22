// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/can_make_payment_query_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/payments/core/can_make_payment_query.h"

namespace payments {

// static
CanMakePaymentQueryFactory* CanMakePaymentQueryFactory::GetInstance() {
  return base::Singleton<CanMakePaymentQueryFactory>::get();
}

CanMakePaymentQuery* CanMakePaymentQueryFactory::GetForContext(
    content::BrowserContext* context) {
  return static_cast<CanMakePaymentQuery*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CanMakePaymentQueryFactory::CanMakePaymentQueryFactory()
    : BrowserContextKeyedServiceFactory(
          "CanMakePaymentQuery",
          BrowserContextDependencyManager::GetInstance()) {}

CanMakePaymentQueryFactory::~CanMakePaymentQueryFactory() {}

content::BrowserContext* CanMakePaymentQueryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Create a separate instance of the service for the Incognito context.
  return context;
}

std::unique_ptr<KeyedService>
CanMakePaymentQueryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<CanMakePaymentQuery>();
}

}  // namespace payments
