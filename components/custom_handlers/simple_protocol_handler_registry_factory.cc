// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace custom_handlers {

// static
SimpleProtocolHandlerRegistryFactory*
SimpleProtocolHandlerRegistryFactory::GetInstance() {
  static base::NoDestructor<SimpleProtocolHandlerRegistryFactory> factory;
  return factory.get();
}

// static
ProtocolHandlerRegistry*
SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  return static_cast<ProtocolHandlerRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, create));
}

SimpleProtocolHandlerRegistryFactory::SimpleProtocolHandlerRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "ProtocolHandlerRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

// Will be created when initializing profile_io_data, so we might
// as well have the framework create this along with other
// PKSs to preserve orderly civic conduct :)
bool SimpleProtocolHandlerRegistryFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

// Do not create this service for tests. MANY tests will fail
// due to the threading requirements of this service. ALSO,
// not creating this increases test isolation (which is GOOD!)
bool SimpleProtocolHandlerRegistryFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
SimpleProtocolHandlerRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // We can't ensure the UserPref has been set, so we pass a nullptr
  // PrefService.
  return custom_handlers::ProtocolHandlerRegistry::Create(
      nullptr, std::make_unique<TestProtocolHandlerRegistryDelegate>());
}

}  // namespace custom_handlers
