// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/dependency_manager.h"

#include <ostream>
#include <string>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/features_buildflags.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/keyed_service/core/keyed_service_factory.h"
#include "components/keyed_service/core/refcounted_keyed_service_factory.h"

#ifndef NDEBUG
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#endif  // NDEBUG

namespace {

// An ordered container of pointers to DependencyNode. The order depends
// on the operation to perform (initialisation, destruction, ...).
using OrderedDependencyNodes =
    std::vector<raw_ptr<DependencyNode, VectorExperimental>>;

// Convert a vector of pointers to DependencyNode to a vector of pointers to
// KeyedServiceBaseFactory.
std::vector<raw_ptr<KeyedServiceBaseFactory, VectorExperimental>>
OrderedFactoriesFromOrderedDependencyNodes(
    const OrderedDependencyNodes& ordered_dependency_nodes) {
  std::vector<raw_ptr<KeyedServiceBaseFactory, VectorExperimental>>
      ordered_factories;
  ordered_factories.reserve(ordered_dependency_nodes.size());
  for (DependencyNode* dependency_node : ordered_dependency_nodes) {
    ordered_factories.push_back(
        static_cast<KeyedServiceBaseFactory*>(dependency_node));
  }
  return ordered_factories;
}

}  // namespace

DependencyManager::DependencyManager() = default;

DependencyManager::~DependencyManager() = default;

void DependencyManager::AddComponent(KeyedServiceBaseFactory* component) {
#if DCHECK_IS_ON()
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_REGISTRATION)
  const bool registration_allowed = (context_created_count_ == 0);
#else
  // TODO(crbug.com/40158018): Tighten this check to ensure that no factories
  // are registered after CreateContextServices() is called.
  const bool registration_allowed =
      !any_context_created_ || !(component->ServiceIsCreatedWithContext() ||
                                 component->ServiceIsNULLWhileTesting());
#endif
  DCHECK(registration_allowed)
      << "Tried to construct " << component->name()
      << " after context.\n"
         "Keyed Service Factories must be constructed before any context is "
         "created. Typically this is done by calling FooFactory::GetInstance() "
         "for all factories in a method called "
         "Ensure.*KeyedServiceFactoriesBuilt().";
#endif  // DCHECK_IS_ON()

  if (disallow_factory_registration_) {
    SCOPED_CRASH_KEY_STRING32("KeyedServiceFactories", "factory_name",
                              component->name());
    base::debug::DumpWithoutCrashing();

    DCHECK(false)
        << "Trying to register KeyedService Factory: `" << component->name()
        << "` after the call to the main registration function `"
        << registration_function_name_error_message_
        << "`. Please add a "
           "call your factory `KeyedServiceFactory::GetInstance()` in the "
           "previous method or to the appropriate "
           "`EnsureBrowserContextKeyedServiceFactoriesBuilt()` function to "
           "properly register your factory.";
  }

  dependency_graph_.AddNode(component);
}

void DependencyManager::RemoveComponent(KeyedServiceBaseFactory* component) {
  dependency_graph_.RemoveNode(component);
}

void DependencyManager::AddEdge(KeyedServiceBaseFactory* depended,
                                KeyedServiceBaseFactory* dependee) {
  dependency_graph_.AddEdge(depended, dependee);
}

void DependencyManager::RegisterPrefsForServices(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED_IN_MIGRATION();
  }

  for (DependencyNode* dependency_node : construction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->RegisterPrefs(pref_registry);
  }
}

void DependencyManager::CreateContextServices(void* context,
                                              bool is_testing_context) {
  AssertContextWasntDestroyed(context);

  const OrderedFactories construction_order = GetConstructionOrder();

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  for (KeyedServiceBaseFactory* factory : construction_order) {
    factory->ContextInitialized(context, is_testing_context);
  }

  for (KeyedServiceBaseFactory* factory : construction_order) {
    if (factory->ServiceIsCreatedWithContext()) {
      factory->CreateServiceNow(context);
    }
  }
}

void DependencyManager::DestroyContextServices(void* context) {
  const OrderedFactories destruction_order = GetDestructionOrder();

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  ShutdownFactoriesInOrder(context, destruction_order);
  MarkContextDead(context);
  DestroyFactoriesInOrder(context, destruction_order);

  const size_t context_service_count =
      KeyedServiceFactory::GetServicesCount(context) +
      RefcountedKeyedServiceFactory::GetServicesCount(context);
  // At this point all services for a specific context should be destroyed.
  // If this is not the case, it means that a service was created but not
  // destroyed properly, potentially due to a wrong dependency declaration
  DCHECK_EQ(context_service_count, 0u);
}

// static
void DependencyManager::PerformInterlockedTwoPhaseShutdown(
    DependencyManager* dependency_manager1,
    void* context1,
    DependencyManager* dependency_manager2,
    void* context2) {
  const OrderedFactories destruction_order1 =
      dependency_manager1->GetDestructionOrder();
  const OrderedFactories destruction_order2 =
      dependency_manager2->GetDestructionOrder();

#ifndef NDEBUG
  dependency_manager1->DumpContextDependencies(context1);
  dependency_manager2->DumpContextDependencies(context2);
#endif

  ShutdownFactoriesInOrder(context1, destruction_order1);
  ShutdownFactoriesInOrder(context2, destruction_order2);

  dependency_manager1->MarkContextDead(context1);
  dependency_manager2->MarkContextDead(context2);

  DestroyFactoriesInOrder(context1, destruction_order1);
  DestroyFactoriesInOrder(context2, destruction_order2);
}

DependencyManager::OrderedFactories DependencyManager::GetConstructionOrder() {
  OrderedDependencyNodes construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED_IN_MIGRATION();
  }
  return OrderedFactoriesFromOrderedDependencyNodes(construction_order);
}

DependencyManager::OrderedFactories DependencyManager::GetDestructionOrder() {
  OrderedDependencyNodes destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order))
    NOTREACHED_IN_MIGRATION();
  return OrderedFactoriesFromOrderedDependencyNodes(destruction_order);
}

void DependencyManager::ShutdownFactoriesInOrder(
    void* context,
    const OrderedFactories& factories) {
  for (KeyedServiceBaseFactory* factory : factories) {
    factory->ContextShutdown(context);
  }
}

void DependencyManager::DestroyFactoriesInOrder(
    void* context,
    const OrderedFactories& factories) {
  for (KeyedServiceBaseFactory* factory : factories) {
    factory->ContextDestroyed(context);
  }
}

void DependencyManager::AssertContextWasntDestroyed(void* context) const {
  if (dead_context_pointers_.find(context) != dead_context_pointers_.end()) {
    // We want to see all possible use-after-destroy in production environment.
    CHECK(false) << "Attempted to access a context that was ShutDown(). "
                 << "This is most likely a heap smasher in progress. After "
                 << "KeyedService::Shutdown() completes, your service MUST "
                 << "NOT refer to depended services again.";
  }
}

void DependencyManager::MarkContextLive(void* context) {
#if DCHECK_IS_ON()
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_REGISTRATION)
  ++context_created_count_;
#else
  any_context_created_ = true;
#endif
#endif

  dead_context_pointers_.erase(context);

  const OrderedFactories construction_order = GetConstructionOrder();

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  for (KeyedServiceBaseFactory* factory : construction_order) {
    factory->ContextCreated(context);
  }
}

void DependencyManager::MarkContextDead(void* context) {
  dead_context_pointers_.insert(context);

#if DCHECK_IS_ON()
#if BUILDFLAG(KEYED_SERVICE_HAS_TIGHT_REGISTRATION)
  CHECK_GT(context_created_count_, 0u);
  --context_created_count_;
#endif
#endif
}

#ifndef NDEBUG
namespace {

std::string KeyedServiceBaseFactoryGetNodeName(DependencyNode* node) {
  return static_cast<KeyedServiceBaseFactory*>(node)->name();
}

}  // namespace

void DependencyManager::DumpDependenciesAsGraphviz(
    const std::string& top_level_name,
    const base::FilePath& dot_file) const {
  DCHECK(!dot_file.empty());
  std::string contents = dependency_graph_.DumpAsGraphviz(
      top_level_name, base::BindRepeating(&KeyedServiceBaseFactoryGetNodeName));
  base::WriteFile(dot_file, contents);
}
#endif  // NDEBUG

DependencyGraph& DependencyManager::GetDependencyGraphForTesting() {
  return dependency_graph_;
}

void DependencyManager::DisallowKeyedServiceFactoryRegistration(
    const std::string& registration_function_name_error_message) {
  disallow_factory_registration_ = true;
  registration_function_name_error_message_ =
      registration_function_name_error_message;
}
