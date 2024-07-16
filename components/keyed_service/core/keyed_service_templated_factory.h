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
#include "components/keyed_service/core/features_buildflags.h"
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
  using TestingFactory = base::OnceCallback<OwnedServicePtr(void* context)>;

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

#if BUILDFLAG(KEYED_SERVICE_HAS_DEPRECATED_ASSOCIATE_API)
  // Maps `context` to `service` with debug checks to prevent duplication and
  // returns a pointer to `service`.
  //
  // TODO(crbug.com/351120207): Calling this method risks breaking many
  // invariants of KeyedServiceTemplatedFactory. It is exposed because
  // some legacy code use it, do not introduce new usage.
  void Associate(void* context, OwnedServicePtr service);

  // Removes the mapping from `context` to a service.
  // TODO(crbug.com/351120207): Calling this method risks breaking many
  // invariants of KeyedServiceTemplatedFactory. It is exposed because
  // some legacy code use it, do not introduce new usage.
  void Disassociate(void* context);
#endif  // !BUILDFLAG(KEYED_SERVICE_HAS_DEPRECATED_ASSOCIATE_API)

  // Returns a new service that will be associated with `context`.
  virtual OwnedServicePtr BuildServiceInstanceFor(void* context) const = 0;

  // KeyedServiceBaseFactory:
  void ContextCreated(void* context) override;
  void ContextInitialized(void* context, bool is_testing_context) override;
  void ContextShutdown(void* context) override;
  void ContextDestroyed(void* context) override;

  bool IsServiceCreated(void* context) const override;
  void CreateServiceNow(void* context) override;

 private:
  // Stage of the context and (context => service) mapping.
  //
  // The progression of the stage goes in order, and it is not possible to go
  // back to a previous stage.
  //
  // Note: the strict progression of the MappingStage is only true and enforced
  // if KEYED_SERVICE_HAS_TIGHT_INITIALIZATION is enabled. If not, the stage is
  // still used to know if a service has been associated with the context, and
  // to know if the context has been created (which prevents associating a new
  // service).
  //
  // This means that if KEYED_SERVICE_HAS_TIGHT_INITIALIZATION is disabled,
  // the only meaningful stages are kServiceAssociated and kServiceShutdown
  // (the other stage are used, but the migrations are not strict).
  //
  // TODO(crbug.com/342112724): remove the note above when all platform enable
  // KEYED_SERVICE_HAS_TIGHT_INITIALIZATION (and the bug is closed).
  enum class MappingStage {
    // The context has been created, but it is not fully initialized  yet. It is
    // not possible to associate a service yet unless the factory overrides
    // ServiceIsRequiredForContextInitialization().
    //
    // Transition to kContextInitialized or kServiceAssociated.
    kContextCreated,

    // The context has been fully initialized but no service associated yet.
    // Calling `GetServiceForContext(..., true)` is valid and will associate the
    // service.
    //
    // Transition to kServiceAssociated when `GetServiceForContext(..., true)`
    // is called or to kServiceShutdown directly if the context is destroyed
    // before any service is associated.
    kContextInitialized,

    // The context is associated with a service (possibly null).
    //
    // Transition to kServiceShutdown as part of the first stage of the two
    // phases shutdown (see ContextShutdown()/ContextDestroyed() for more
    // details).
    kServiceAssociated,

    // This corresponds to the first stage of the two phase shutdown. It is
    // valid to access the mapping but if no association has been made by this
    // point, a null service will be returned.
    //
    // Transition to kServiceDestroyed as part of the second stage of the two
    // phases shutdown (see ContextShutdown()/ContextDestroyed() for more
    // details).
    kServiceShutdown,

    // This corresponds to the second stage of the two phase shutdown. It is
    // invalid to access the mapping. The factory still know about the context
    // until all factories have gone through the destroy phase, then it will
    // forget about the context.
    //
    // No transition possible.
    kServiceDestroyed,
  };

  // Stores the information about a mapping.
  struct MappingInfo {
    // Explicitly declares the constructors as they cannot be inline due to
    // their complexity.
    MappingInfo();

    MappingInfo(const MappingInfo&) = delete;
    MappingInfo& operator=(const MappingInfo&) = delete;

    MappingInfo(MappingInfo&&);
    MappingInfo& operator=(MappingInfo&&);

    ~MappingInfo();

    // The current stage of the mapping.
    MappingStage stage = MappingStage::kContextCreated;

    // The associated service (valid only if stage >= kServiceAssociated).
    OwnedServicePtr service;

    // The testing factory to use. If null, then no factory has been set,
    // otherwise, will be used to create the service instead of calling
    // the method CreateServiceNow().
    TestingFactory testing_factory;

    // Whether the context is a test object.
    bool is_testing_context = false;
  };

  // Mapping between a context and the information about it.
  using Mapping = base::flat_map<const void*, MappingInfo>;

  // Returns the iterator to the mapping for `context`, creating it if missing
  // (only when KEYED_SERVICE_HAS_TIGHT_REGISTRATION is disabled). Once all
  // platforms set KEYED_SERVICE_HAS_TIGHT_REGISTRATION, this method should be
  // removed and all uses replaced by `mapping_.find(context)`.
  Mapping::iterator FindOrCreateMapping(void* context);

  // Returns the map from context to the number of services instantiated.
  static base::flat_map<const void*, size_t>& GetServicesCountMap();

  // Mapping between a context and the information about it.
  Mapping mapping_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_TEMPLATED_FACTORY_H_
