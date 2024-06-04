// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_TEMPLATED_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_TEMPLATED_FACTORY_H_

#include <stddef.h>

#include <memory>
#include <type_traits>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_scoped_refptr_mismatch_checker.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/keyed_service/core/keyed_service_export.h"

// Templated sub-class for KeyedServiceBaseFactory.
//
// This allow to share the implementation between KeyedService factories and
// RefcountedKeyedService factories without any duplication. Code should not
// directly inherits from those, but instead should inherit from sub-classes
// that specialize the `context` type.
template <typename ServiceType>
class KEYED_SERVICE_EXPORT KeyedServiceTemplatedFactory
    : public KeyedServiceBaseFactory {
 public:
  // Returns the number of KeyedServices that are currently active for
  // a given context.
  static size_t GetServicesCount(const void* context);

 protected:
  // Non-owning pointer to `ServiceType`.
  using ServicePtr =
      std::conditional_t<base::internal::IsRefCountedType<ServiceType>,
                         scoped_refptr<ServiceType>,
                         ServiceType*>;

  // Owning pointer to `ServiceType`.
  using OwnedServicePtr =
      std::conditional_t<base::internal::IsRefCountedType<ServiceType>,
                         scoped_refptr<ServiceType>,
                         std::unique_ptr<ServiceType>>;

  // A callback that creates the instance of a KeyedService for a given
  // context. This is primarily used for testing, where we want to feed
  // a specific test double into the system.
  using TestingFactory =
      base::RepeatingCallback<OwnedServicePtr(void* context)>;

  // Inherit super class constructor.
  using KeyedServiceBaseFactory::KeyedServiceBaseFactory;

  ~KeyedServiceTemplatedFactory() override;

  // Associates `testing_factory` with `context` so that `testing_factory` is
  // used to create the service when requested. `testing_factory` may be null
  // to signal that the service should ne null. Multiple calls to this method
  // are allowed; previous service will be shutdown.
  void SetTestingFactory(void* context, TestingFactory testing_factory);

  // Associates `testing_factory` with `context` and immediately returns the
  // created service. Since the factory will be used immediately, it may not
  // be empty.
  ServicePtr SetTestingFactoryAndUse(void* context,
                                     TestingFactory testing_factory);

  // Common implementation that maps `context` to some service object. Deals
  // with incognito contexts per subclasses instruction with GetContextToUse()
  // method on the base.  If `create` is true, the service will be created
  // using BuildServiceInstanceFor() if it doesn't already exists.
  ServicePtr GetServiceForContext(void* context, bool created);

  // Maps `context` to `service` with debug checks to prevent duplication and
  // returns a pointer to `service`.
  ServicePtr Associate(void* context, OwnedServicePtr service);

  // Removes the mapping from `context` to a service.
  void Disassociate(void* context);

  // Returns a new service that will be associated with `context`.
  virtual OwnedServicePtr BuildServiceInstanceFor(void* context) const = 0;

  // Returns whether the `context` is off-the-record or not.
  virtual bool IsOffTheRecord(void* context) const = 0;

  // KeyedServiceBaseFactory:
  void ContextShutdown(void* context) override;
  void ContextDestroyed(void* context) override;

  void SetEmptyTestingFactory(void* context) override;
  bool HasTestingFactory(void* context) const override;
  bool IsServiceCreated(void* context) const override;
  void CreateServiceNow(void* context) override;

 private:
  // Returns the map from context to the number of services instantiated.
  static base::flat_map<const void*, size_t>& GetServicesCountMap();

  // The mapping between a context and its service.
  base::flat_map<void*, OwnedServicePtr> mapping_;

  // The mapping between a context and its overridden TestingFactory.
  base::flat_map<void*, TestingFactory> testing_factories_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_TEMPLATED_FACTORY_H_
