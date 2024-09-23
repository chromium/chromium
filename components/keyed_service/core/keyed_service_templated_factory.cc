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
#include "components/keyed_service/core/features_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_keyed_service.pbzero.h"

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::MappingInfo::MappingInfo() = default;

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::MappingInfo::MappingInfo(
    MappingInfo&&) = default;

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::MappingInfo&
KeyedServiceTemplatedFactory<ServiceType>::MappingInfo::operator=(
    MappingInfo&&) = default;

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::MappingInfo::~MappingInfo() =
    default;

// static
template <typename ServiceType>
size_t KeyedServiceTemplatedFactory<ServiceType>::GetServicesCount(
    const void* context) {
  const auto& services_count = GetServicesCountMap();
  const auto iterator = services_count.find(context);
  return iterator != services_count.end() ? iterator->second : 0;
}

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::~KeyedServiceTemplatedFactory() =
    default;

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::SetTestingFactory(
    void* context,
    TestingFactory testing_factory) {
  // Ensure that caller has correctly marked the context as live before
  // calling this method.
  AssertContextWasntDestroyed(context);

  auto iterator = FindOrCreateMapping(context);
  CHECK(iterator != mapping_.end());

  // We may have to go through the shutdown and destroy mechanisms because
  // there are unit tests that create a service on a context and then change
  // the testing service mid-test.
  if (iterator->second.stage >= MappingStage::kServiceAssociated) {
    // The `iterator` will be invalidated by `ContextDestroyed(...)` so copy
    // the `is_testing_context` boolean until `ContextInitialized(...)` is
    // called.
    const bool is_testing_context = iterator->second.is_testing_context;

    ContextShutdown(context);
    ContextDestroyed(context);

    ContextCreated(context);
    ContextInitialized(context, is_testing_context);

    // Update the iterator since the mapping has been destroyed and
    // recreated thus invalidating the iterator.
    iterator = mapping_.find(context);
  }

  // If the factory is null, install a factory returning a null service.
  if (!testing_factory) {
    testing_factory =
        base::BindOnce([](void* context) { return OwnedServicePtr{}; });
  }

  CHECK(testing_factory);
  iterator->second.testing_factory = std::move(testing_factory);
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

  auto iterator = FindOrCreateMapping(context);
  CHECK(iterator != mapping_.end());

  MappingInfo& info = iterator->second;
  CHECK_LT(info.stage, MappingStage::kServiceDestroyed);

  if (info.stage < MappingStage::kServiceAssociated) {
    CHECK(!info.service);

    // Object not found. Is creation allowed?
    if (!create) {
      return nullptr;
    }

#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_INITIALIZATION)
    // Only allowed to create KeyedService if the context is fully initialized
    // or if the factory overrides ServiceIsRequiredForContextInitialization().
    CHECK(info.stage == MappingStage::kContextInitialized ||
          ServiceIsRequiredForContextInitialization());
#else
    CHECK_LT(info.stage, MappingStage::kServiceAssociated);
#endif

    // Create new object, using the testing factory if set.
    info.stage = MappingStage::kServiceAssociated;
    if (info.testing_factory) {
      info.service = std::move(info.testing_factory).Run(context);
    } else if (info.is_testing_context && ServiceIsNULLWhileTesting()) {
      // Do not create the service if the context is a testing context and
      // the factory is configured to not create the service in that case.
    } else {
      info.service = BuildServiceInstanceFor(context);
    }
  }

  CHECK_GE(info.stage, MappingStage::kServiceAssociated);
  if constexpr (std::same_as<ServicePtr, OwnedServicePtr>) {
    return info.service;
  } else {
    return info.service.get();
  }
}

#if BUILDFLAG(KEYED_SERVICE_HAS_DEPRECATED_ASSOCIATE_API)

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::Associate(
    void* context,
    OwnedServicePtr service) {
  auto iterator = FindOrCreateMapping(context);
  CHECK(iterator != mapping_.end());

  iterator->second.stage = MappingStage::kServiceAssociated;
  iterator->second.service = std::move(service);
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::Disassociate(void* context) {
  auto iterator = FindOrCreateMapping(context);
  CHECK(iterator != mapping_.end());

  iterator->second.stage = MappingStage::kContextInitialized;
  iterator->second.service = nullptr;
}

#endif

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::ContextCreated(void* context) {
  auto insertion_result =
      mapping_.insert(std::make_pair(context, MappingInfo{}));
  CHECK(insertion_result.second);
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::ContextInitialized(
    void* context,
    bool is_testing_context) {
  auto iterator = FindOrCreateMapping(context);
  CHECK(iterator != mapping_.end());

  iterator->second.is_testing_context = is_testing_context;
  if (iterator->second.stage == MappingStage::kContextCreated) {
    iterator->second.stage = MappingStage::kContextInitialized;
  } else {
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_INITIALIZATION)
    CHECK(iterator->second.stage == MappingStage::kServiceAssociated &&
          ServiceIsRequiredForContextInitialization());
#else
    CHECK_LE(iterator->second.stage, MappingStage::kServiceAssociated);
#endif
  }
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::ContextShutdown(void* context) {
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_REGISTRATION)
  auto iterator = mapping_.find(context);
  CHECK(iterator != mapping_.end());
#else
  auto iterator = mapping_.find(context);
  if (iterator == mapping_.end()) {
    // If the mapping is missing, it means the factory has been registered
    // after the context has been created, but no instance of the service
    // has been created. Return early as there is nothing to do.
    return;
  }
#endif

#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_INITIALIZATION)
  CHECK_LE(iterator->second.stage, MappingStage::kServiceAssociated);
#else
  CHECK_LT(iterator->second.stage, MappingStage::kServiceShutdown);
#endif

  iterator->second.stage = MappingStage::kServiceShutdown;
  if (iterator->second.service) {
    if constexpr (base::internal::IsRefCountedType<ServiceType>) {
      iterator->second.service->ShutdownOnUIThread();
    } else {
      iterator->second.service->Shutdown();
    }
  }
}

template <typename ServiceType>
void KeyedServiceTemplatedFactory<ServiceType>::ContextDestroyed(
    void* context) {
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_REGISTRATION)
  auto iterator = mapping_.find(context);
  CHECK(iterator != mapping_.end());
#else
  auto iterator = mapping_.find(context);
  if (iterator == mapping_.end()) {
    // If the mapping is missing, it means the factory has been registered
    // after the context has been created, but no instance of the service
    // has been created. Return early as there is nothing to do.
    return;
  }
#endif

  const MappingStage stage = iterator->second.stage;
  CHECK(stage == MappingStage::kServiceShutdown);

  // Set the stage to kServiceDestroyed to catch any code trying to access the
  // service via the KeyedServiceFactory while it is destroyed, then drop the
  // reference to the service (which should hopefully destroy it). Finally
  // remove the mapping.
  iterator->second.stage = MappingStage::kServiceDestroyed;
  iterator->second.service = OwnedServicePtr();
  mapping_.erase(iterator);

  KeyedServiceBaseFactory::ContextDestroyed(context);
}

template <typename ServiceType>
bool KeyedServiceTemplatedFactory<ServiceType>::IsServiceCreated(
    void* context) const {
  auto iterator = mapping_.find(context);
  return iterator != mapping_.end() && iterator->second.service;
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

template <typename ServiceType>
KeyedServiceTemplatedFactory<ServiceType>::Mapping::iterator
KeyedServiceTemplatedFactory<ServiceType>::FindOrCreateMapping(void* context) {
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_REGISTRATION)
  return mapping_.find(context);
#else
  // TODO(crbug.com/40158018): when no factories are register after the first
  // context is created, remove this workaround and replace FindOrCreateMapping
  // by `mapping_.find(context)` followed by a CHECK.
  auto iterator = mapping_.find(context);
  if (iterator == mapping_.end()) {
    MappingInfo mapping_info{};
    mapping_info.stage = MappingStage::kContextInitialized;

    auto insertion_result =
        mapping_.insert(std::make_pair(context, std::move(mapping_info)));

    CHECK(insertion_result.second);
    iterator = insertion_result.first;
  }
  return iterator;
#endif
}

// Explicitly instantiate the supported KeyedServiceTemplatedFactory variants.
template class KeyedServiceTemplatedFactory<KeyedService>;
template class KeyedServiceTemplatedFactory<RefcountedKeyedService>;
