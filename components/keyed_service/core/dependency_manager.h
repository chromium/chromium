// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_MANAGER_H_
#define COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_MANAGER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_export.h"

class KeyedServiceBaseFactory;

namespace base {
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// DependencyManager manages dependency between KeyedServiceBaseFactory
// broadcasting the context creation and destruction to each factory in
// a safe order based on the stated dependencies.
class KEYED_SERVICE_EXPORT DependencyManager {
 public:
  // Shuts down all keyed services managed by two
  // DependencyManagers (DMs), then destroys them. The order of execution is:
  // - Shutdown services in DM1
  // - Shutdown services in DM2
  // - Destroy services in DM1
  // - Destroy services in DM2
  static void PerformInterlockedTwoPhaseShutdown(
      DependencyManager* dependency_manager1,
      void* context1,
      DependencyManager* dependency_manager2,
      void* context2);

 protected:
  DependencyManager();
  virtual ~DependencyManager();

  // Adds/Removes a component from our list of live components. Removing will
  // also remove live dependency links.
  void AddComponent(KeyedServiceBaseFactory* component);
  void RemoveComponent(KeyedServiceBaseFactory* component);

  // Adds a dependency between two factories.
  void AddEdge(KeyedServiceBaseFactory* depended,
               KeyedServiceBaseFactory* dependee);

  // Registers preferences for all services via |registry|.
  void RegisterPrefsForServices(user_prefs::PrefRegistrySyncable* registry);

  // Called upon creation of |context| to create services that want to be
  // started at the creation of a context and register service-related
  // preferences.
  //
  // To have a KeyedService started when a context is created the method
  // KeyedServiceBaseFactory::ServiceIsCreatedWithContext() must be overridden
  // to return true.
  //
  // If |is_testing_context| then the service will not be started unless the
  // method KeyedServiceBaseFactory::ServiceIsNULLWhileTesting() return false.
  void CreateContextServices(void* context, bool is_testing_context);

  // Called upon destruction of |context| to destroy all services associated
  // with it.
  void DestroyContextServices(void* context);

  // Runtime assertion called as a part of GetServiceForContext() to check if
  // |context| is considered stale. This will NOTREACHED() or
  // base::debug::DumpWithoutCrashing() depending on the DCHECK_IS_ON() value.
  void AssertContextWasntDestroyed(void* context) const;

  // Marks |context| as live (i.e., not stale). This method can be called as a
  // safeguard against |AssertContextWasntDestroyed()| checks going off due to
  // |context| aliasing an instance from a prior construction (i.e., 0xWhatever
  // might be created, be destroyed, and then a new object might be created at
  // 0xWhatever).
  void MarkContextLive(void* context);

  // Marks |context| as dead (i.e., stale). Calls passing |context| to
  //|AssertContextWasntDestroyed()| will flag an error until that context is
  // marked as live again with MarkContextLive().
  void MarkContextDead(void* context);

#ifndef NDEBUG
  // Dumps service dependency graph as a Graphviz dot file |dot_file| with a
  // title |top_level_name|. Helper for |DumpContextDependencies|.
  void DumpDependenciesAsGraphviz(const std::string& top_level_name,
                                  const base::FilePath& dot_file) const;
#endif  // NDEBUG

 private:
  friend class KeyedServiceBaseFactory;

#ifndef NDEBUG
  // Hook for subclass to dump the dependency graph of service for |context|.
  virtual void DumpContextDependencies(void* context) const = 0;
#endif  // NDEBUG

  std::vector<DependencyNode*> GetDestructionOrder();
  static void ShutdownFactoriesInOrder(void* context,
                                       std::vector<DependencyNode*>& order);
  static void DestroyFactoriesInOrder(void* context,
                                      std::vector<DependencyNode*>& order);

  DependencyGraph dependency_graph_;

  // A list of context objects that have gone through the Shutdown() phase.
  // These pointers are most likely invalid, but we keep track of their
  // locations in memory so we can nicely assert if we're asked to do anything
  // with them.
  std::set<void*> dead_context_pointers_;

  DISALLOW_COPY_AND_ASSIGN(DependencyManager);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_MANAGER_H_
