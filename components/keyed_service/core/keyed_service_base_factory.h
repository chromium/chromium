// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_BASE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_BASE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/dependency_node.h"
#include "components/keyed_service/core/keyed_service_export.h"

class DependencyManager;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Base class for factories that take an opaque pointer and return some service.
// Not for direct usage, instead use descendent classes that deal with more
// specific context objects.
//
// This object describes general dependency management between factories while
// direct subclasses react to lifecycle events and implement memory management.
//
// All factories must have been created at least once before the context is
// created in order to work correctly (see crbug.com/1150733). The standard way
// to do this in a //content-based embedder is to call FooFactory::GetInstance()
// for each factory used by your embedder from your embedder's implementation of
// content::BrowserMainParts::PreMainMessageLoopRun().
class KEYED_SERVICE_EXPORT KeyedServiceBaseFactory : public DependencyNode {
 public:
  // The type is used to determine whether a service can depend on another.
  // Each type can only depend on other services that are of the same type.
  // TODO(crbug.com/40619682): Remove once there are no dependencies between
  // factories with different type of context, or dependencies are safe to have.
  enum Type { BROWSER_CONTEXT, BROWSER_STATE, SIMPLE };

  KeyedServiceBaseFactory(const KeyedServiceBaseFactory&) = delete;
  KeyedServiceBaseFactory& operator=(const KeyedServiceBaseFactory&) = delete;

  // Returns our name.
  const char* name() const { return service_name_; }

  // Returns the type of this service factory.
  // TODO(crbug.com/40619682): Remove once there are no dependencies between
  // factories with different type of context, or dependencies are safe to have.
  Type type() { return type_; }

  // Returns whether the service is created for the given context.
  virtual bool IsServiceCreated(void* context) const = 0;

  // Enforces the creation of a service, used for testing only.
  void CreateServiceNowForTesting(void* context);

 protected:
  KeyedServiceBaseFactory(const char* service_name,
                          DependencyManager* manager,
                          Type type);
  virtual ~KeyedServiceBaseFactory();

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(KeyedServiceBaseFactory* rhs);

  // Runtime assertion to check if |context| is considered stale. Should be used
  // by subclasses when accessing |context|.
  void AssertContextWasntDestroyed(void* context) const;

  // Marks |context| as live (i.e., not stale). This method can be called as a
  // safeguard against |AssertContextWasntDestroyed()| checks going off due to
  // |context| aliasing an instance from a prior construction (i.e., 0xWhatever
  // might be created, be destroyed, and then a new object might be created at
  // 0xWhatever).
  void MarkContextLive(void* context);

  // Finds which context (if any) to use.
  virtual void* GetContextToUse(void* context) const = 0;

  // By default, instance of a service can only be created once the context
  // has been fully initialized. Some embedders uses services as part of
  // their initialisation. Those factories needs to override this method to
  // return true.
  virtual bool ServiceIsRequiredForContextInitialization() const;

  // By default, instance of a service are created lazily when GetForContext()
  // is called by the subclass. Some services need to be created as soon as the
  // context is created and should override this method to return true.
  virtual bool ServiceIsCreatedWithContext() const;

  // By default, testing contexts will be treated like normal contexts. If this
  // method is overridden to return true, then the service associated with the
  // testing context will be null.
  virtual bool ServiceIsNULLWhileTesting() const;

  // Most of the KeyedServices needs a fully initialised context, but some
  // embedders delegates part of the context initialisation to services. To
  // support this, the KeyedServiceFactories are informed of the creation
  // and the complete initialisation of the context. Creation of services is
  // by default disabled until the context has been fully initialised.
  virtual void ContextCreated(void* context) = 0;
  virtual void ContextInitialized(void* context, bool is_testing_context) = 0;

  // The service build by the factories goes through a two phase shutdown.
  // It is up to the individual factory types to determine what this two pass
  // shutdown means. The general framework guarantees the following:
  //
  // - Each ContextShutdown() is called in dependency order (and you may
  //   reach out to other services during this phase).
  //
  // - Each ContextDestroyed() is called in dependency order. Accessing a
  //   service with GetForContext() will NOTREACHED() and code should delete/
  //   deref/do other final memory management during this phase. The base class
  //   method *must* be called as the last thing.
  virtual void ContextShutdown(void* context) = 0;
  virtual void ContextDestroyed(void* context);

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  friend class DependencyManager;

  // The DependencyManager used. In real code, this will be a singleton used
  // by all the factories of a given type. Unit tests will use their own copy.
  raw_ptr<DependencyManager> dependency_manager_;

  // Registers any preferences used by this service. This should be overridden
  // by any services that want to register context-specific preferences.
  virtual void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {}

  // Create the service associated with |context|.
  virtual void CreateServiceNow(void* context) = 0;

  // A static string passed in to the constructor. Should be unique across all
  // services.
  const char* service_name_;

  // The type of this service.
  // TODO(crbug.com/40619682): Remove once there are no dependencies between
  // factories with different type of context, or dependencies are safe to have.
  Type type_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_BASE_FACTORY_H_
