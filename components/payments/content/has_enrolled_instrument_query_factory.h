// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_HAS_ENROLLED_INSTRUMENT_QUERY_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_HAS_ENROLLED_INSTRUMENT_QUERY_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace payments {

class HasEnrolledInstrumentQuery;

// Ensures that there's only one instance of HasEnrolledInstrumentQuery per
// browser context.
class HasEnrolledInstrumentQueryFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static HasEnrolledInstrumentQueryFactory* GetInstance();
  HasEnrolledInstrumentQuery* GetForContext(content::BrowserContext* context);

  HasEnrolledInstrumentQueryFactory(const HasEnrolledInstrumentQueryFactory&) =
      delete;
  HasEnrolledInstrumentQueryFactory& operator=(
      const HasEnrolledInstrumentQueryFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<HasEnrolledInstrumentQueryFactory>;

  HasEnrolledInstrumentQueryFactory();
  ~HasEnrolledInstrumentQueryFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_HAS_ENROLLED_INSTRUMENT_QUERY_FACTORY_H_
