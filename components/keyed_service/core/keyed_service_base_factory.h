// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_BASE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_BASE_FACTORY_H_

#include <set>

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
class KEYED_SERVICE_EXPORT KeyedServiceBaseFactory : public DependencyNode {
 public:
  // The type is used to determine whether a service can depend on another.
  // Each type can only depend on other services that are of the same type.
  // TODO(crbug.com/944906): Remove once there are no dependencies between
  // factories with different type of context, or dependencies are safe to have.
  enum Type { BROWSER_CONTEXT, BROWSER_STATE, SIMPLE };

  // Returns our name.
  const char* name() const { return service_name_; }

  // Returns the type of this service factory.
  // TODO(crbug.com/944906): Remove once there are no dependencies between
  // factories with different type of context, or dependencies are safe to have.
  Type type() { return type_; }

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

  // By default, instance of a service are created lazily when GetForContext()
  // is called by the subclass. Some services need to be created as soon as the
  // context is created and should override this method to return true.
  virtual bool ServiceIsCreatedWithContext() const;

  // By default, testing contexts will be treated like normal contexts. If this
  // method is overriden to return true, then the service associated with the
  // testing context will be null.
  virtual bool ServiceIsNULLWhileTesting() const;

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
  DependencyManager* dependency_manager_;

  // Registers any preferences used by this service. This should be overriden by
  // any services that want to register context-specific preferences.
  virtual void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {}

  // Used by DependencyManager to disable creation of the service when the
  // method ServiceIsNULLWhileTesting() returns true.
  virtual void SetEmptyTestingFactory(void* context) = 0;

  // Returns true if a testing factory function has been set for |context|.
  virtual bool HasTestingFactory(void* context) = 0;

  // Create the service associated with |context|.
  virtual void CreateServiceNow(void* context) = 0;

  // A static string passed in to the constructor. Should be unique across all
  // services.
  const char* service_name_;

  // The type of this service.
  // TODO(crbug.com/944906): Remove once there are no dependencies between
  // factories with different type of context, or dependencies are safe to have.
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(KeyedServiceBaseFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_BASE_FACTORY_H_
