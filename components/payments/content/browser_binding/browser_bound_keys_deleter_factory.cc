// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_keys_deleter_factory.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"

namespace payments {

// static
BrowserBoundKeyDeleterFactory* BrowserBoundKeyDeleterFactory::GetInstance() {
  static base::NoDestructor<BrowserBoundKeyDeleterFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
BrowserBoundKeyDeleterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(https://crbug.com/406299749): Delete invalid browser bound keys.

  // Return an empty service object to avoid holding dependencies forever when
  // this service only runs a clean-up method on initialization.
  return std::make_unique<KeyedService>();
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
