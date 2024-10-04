// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/has_enrolled_instrument_query_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/payments/core/has_enrolled_instrument_query.h"

namespace payments {

// static
HasEnrolledInstrumentQueryFactory*
HasEnrolledInstrumentQueryFactory::GetInstance() {
  return base::Singleton<HasEnrolledInstrumentQueryFactory>::get();
}

HasEnrolledInstrumentQuery* HasEnrolledInstrumentQueryFactory::GetForContext(
    content::BrowserContext* context) {
  return static_cast<HasEnrolledInstrumentQuery*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

HasEnrolledInstrumentQueryFactory::HasEnrolledInstrumentQueryFactory()
    : BrowserContextKeyedServiceFactory(
          "HasEnrolledInstrumentQuery",
          BrowserContextDependencyManager::GetInstance()) {}

HasEnrolledInstrumentQueryFactory::~HasEnrolledInstrumentQueryFactory() =
    default;

content::BrowserContext*
HasEnrolledInstrumentQueryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Create a separate instance of the service for the Incognito context.
  return context;
}

std::unique_ptr<KeyedService>
HasEnrolledInstrumentQueryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HasEnrolledInstrumentQuery>();
}

}  // namespace payments
