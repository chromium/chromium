// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_templated_factory.h"

#include <concepts>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_keyed_service.pbzero.h"

// static
template <typename ServiceType>
size_t KeyedServiceTemplatedFactory<ServiceType>::GetServicesCount(
    const void* context) {
  const auto& services_count = GetServicesCountMap();
  const auto iter = services_count.find(context);
  return iter != services_count.end() ? iter->second : 0;
}

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::~KeyedServiceTemplatedFactory() {
  CHECK(mapping_.empty());
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::SetTestingFactory(
    void* context,
    TestingFactory testing_factory) {
  // Ensure that |context| is not marked as stale (e.g., due to it aliasing an
  // instance that was destroyed in an earlier test) in order to avoid accesses
  // to |context| in |BrowserContextShutdown| from causing
  // |AssertBrowserContextWasntDestroyed| to raise an error.
  MarkContextLive(context);

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a context and then change the
  // testing service mid-test.
  ContextShutdown(context);
  ContextDestroyed(context);

  testing_factories_.emplace(context, std::move(testing_factory));
}

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::ServicePtr
KeyedServiceTemplatedFactory<ServiceType>::SetTestingFactoryAndUse(
    void* context,
    TestingFactory testing_factory) {
  CHECK(testing_factory);
  SetTestingFactory(context, std::move(testing_factory));
  return GetServiceForContext(context, true);
}

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::ServicePtr
KeyedServiceTemplatedFactory<ServiceType>::GetServiceForContext(void* context,
                                                                bool create) {
  TRACE_EVENT("browser,startup", "KeyedServiceFactory::GetServiceForContext",
              [this](perfetto::EventContext ctx) {
                ctx.event()->set_chrome_keyed_service()->set_name(name());
              });
  AssertContextWasntDestroyed(context);
  context = GetContextToUse(context);
  if (!context) {
    return nullptr;
  }

  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end()) {
    if constexpr (std::same_as<ServicePtr, OwnedServicePtr>) {
      return iterator->second;
    } else {
      return iterator->second.get();
    }
  }

  // Object not found.
  if (!create) {
    return nullptr;  // And we're forbidden from creating one.
  }

  // Create new object.
  // Check to see if we have a per-context testing factory that we should use
  // instead of default behavior.
  OwnedServicePtr service;
  if (auto factory_iterator = testing_factories_.find(context);
      factory_iterator != testing_factories_.end()) {
    if (factory_iterator->second) {
      service = factory_iterator->second.Run(context);
    }
  } else {
    service = BuildServiceInstanceFor(context);
  }

  return Associate(context, std::move(service));
}

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::ServicePtr
KeyedServiceTemplatedFactory<ServiceType>::Associate(void* context,
                                                     OwnedServicePtr service) {
  // Only count non-null services.
  if (service) {
    ++GetServicesCountMap()[context];
  }

  auto result = mapping_.emplace(context, std::move(service));
  // If `context` is already in `mapping_`, then something has gone wrong in
  // initializing services.
  CHECK(result.second);

  if constexpr (std::same_as<ServicePtr, OwnedServicePtr>) {
    return result.first->second;
  } else {
    return result.first->second.get();
  }
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::Disassociate(void* context) {
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end()) {
    // If a service was null, it is not considered in the count.
    if (iterator->second) {
      auto& services_count = GetServicesCountMap();
      if (--services_count.at(context) == 0) {
        services_count.erase(context);
      }
    }
    mapping_.erase(iterator);
  }
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::ContextShutdown(void* context) {
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end() && iterator->second) {
    if constexpr (base::internal::IsRefCountedType<ServiceType>) {
      iterator->second->ShutdownOnUIThread();
    } else {
      iterator->second->Shutdown();
    }
  }
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::ContextDestroyed(
    void* context) {
  Disassociate(context);

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  testing_factories_.erase(context);

  KeyedServiceBaseFactory::ContextDestroyed(context);
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::SetEmptyTestingFactory(
    void* context) {
  SetTestingFactory(context, TestingFactory());
}

template <typename ServiceType>
bool KeyedServiceTemplatedFactory<ServiceType>::HasTestingFactory(
    void* context) const {
  return base::Contains(testing_factories_, context);
}

template <typename ServiceType>
bool KeyedServiceTemplatedFactory<ServiceType>::IsServiceCreated(
    void* context) const {
  auto it = mapping_.find(context);
  return it != mapping_.end() && it->second != nullptr;
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::CreateServiceNow(
    void* context) {
  GetServiceForContext(context, true);
}

template <typename ServiceType>
base::flat_map<const void*, size_t>&
KeyedServiceTemplatedFactory<ServiceType>::GetServicesCountMap() {
  static base::NoDestructor<base::flat_map<const void*, size_t>>
      services_count_map;
  return *services_count_map;
}

// Explicitly instantiate the supported KeyedServiceTemplatedFactory variants.
template class KeyedServiceTemplatedFactory<KeyedService>;
template class KeyedServiceTemplatedFactory<RefcountedKeyedService>;
