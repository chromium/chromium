// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_keys_deleter_factory.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/payments/content/browser_binding/browser_bound_key_store_android.h"
#include "components/payments/content/browser_binding/browser_bound_keys_deleter.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/browser_context.h"

namespace payments {

// static
BrowserBoundKeyDeleterFactory* BrowserBoundKeyDeleterFactory::GetInstance() {
  static base::NoDestructor<BrowserBoundKeyDeleterFactory> instance;
  return instance.get();
}

// static
BrowserBoundKeyDeleter* BrowserBoundKeyDeleterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BrowserBoundKeyDeleter*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

void BrowserBoundKeyDeleterFactory::SetServiceForTesting(
    std::unique_ptr<BrowserBoundKeyDeleter> service) {
  service_for_testing_ = std::move(service);
}

content::BrowserContext* BrowserBoundKeyDeleterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    // There is no need to remove invalid BBKs for a derived OTR profile, since
    // it would have been done for the original profile.
    return nullptr;
  }
  return context;
}

std::unique_ptr<KeyedService>
BrowserBoundKeyDeleterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(!context->IsOffTheRecord());
  std::unique_ptr<BrowserBoundKeyDeleter> service;
  if (service_for_testing_) {
    service = std::move(service_for_testing_);
  } else {
    service = std::make_unique<BrowserBoundKeyDeleter>(
        webdata_services::WebDataServiceWrapperFactory::
            GetWebPaymentsWebDataServiceForBrowserContext(
                context, ServiceAccessType::EXPLICIT_ACCESS));
  }
  // This triggers a cleanup of browser bound keys at startup (and the service
  // may be used later for explicit cleanup from delete browsing data).
  service->RemoveInvalidBBKs();
  return service;
}

bool BrowserBoundKeyDeleterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

BrowserBoundKeyDeleterFactory::BrowserBoundKeyDeleterFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowserBoundKeyDeleter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(webdata_services::WebDataServiceWrapperFactory::GetInstance());
}

BrowserBoundKeyDeleterFactory::~BrowserBoundKeyDeleterFactory() = default;

}  // namespace payments
