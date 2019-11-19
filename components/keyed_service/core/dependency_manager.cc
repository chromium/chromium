// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/dependency_manager.h"

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"

#ifndef NDEBUG
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#endif  // NDEBUG

DependencyManager::DependencyManager() {
}

DependencyManager::~DependencyManager() {
}

void DependencyManager::AddComponent(KeyedServiceBaseFactory* component) {
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
  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

  for (auto* dependency_node : construction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->RegisterPrefs(pref_registry);
  }
}

void DependencyManager::CreateContextServices(void* context,
                                              bool is_testing_context) {
  MarkContextLive(context);

  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  for (auto* dependency_node : construction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    if (is_testing_context && factory->ServiceIsNULLWhileTesting() &&
        !factory->HasTestingFactory(context)) {
      factory->SetEmptyTestingFactory(context);
    } else if (factory->ServiceIsCreatedWithContext()) {
      factory->CreateServiceNow(context);
    }
  }
}

void DependencyManager::DestroyContextServices(void* context) {
  std::vector<DependencyNode*> destruction_order = GetDestructionOrder();

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  ShutdownFactoriesInOrder(context, destruction_order);
  MarkContextDead(context);
  DestroyFactoriesInOrder(context, destruction_order);
}

// static
void DependencyManager::PerformInterlockedTwoPhaseShutdown(
    DependencyManager* dependency_manager1,
    void* context1,
    DependencyManager* dependency_manager2,
    void* context2) {
  std::vector<DependencyNode*> destruction_order1 =
      dependency_manager1->GetDestructionOrder();
  std::vector<DependencyNode*> destruction_order2 =
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

std::vector<DependencyNode*> DependencyManager::GetDestructionOrder() {
  std::vector<DependencyNode*> destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order))
    NOTREACHED();
  return destruction_order;
}

void DependencyManager::ShutdownFactoriesInOrder(
    void* context,
    std::vector<DependencyNode*>& order) {
  for (auto* dependency_node : order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->ContextShutdown(context);
  }
}

void DependencyManager::DestroyFactoriesInOrder(
    void* context,
    std::vector<DependencyNode*>& order) {
  for (auto* dependency_node : order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->ContextDestroyed(context);
  }
}

void DependencyManager::AssertContextWasntDestroyed(void* context) const {
  if (dead_context_pointers_.find(context) != dead_context_pointers_.end()) {
#if DCHECK_IS_ON()
    NOTREACHED() << "Attempted to access a context that was ShutDown(). "
                 << "This is most likely a heap smasher in progress. After "
                 << "KeyedService::Shutdown() completes, your service MUST "
                 << "NOT refer to depended services again.";
#else   // DCHECK_IS_ON()
    // We want to see all possible use-after-destroy in production environment.
    base::debug::DumpWithoutCrashing();
#endif  // DCHECK_IS_ON()
  }
}

void DependencyManager::MarkContextLive(void* context) {
  dead_context_pointers_.erase(context);
}

void DependencyManager::MarkContextDead(void* context) {
  dead_context_pointers_.insert(context);
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
      top_level_name, base::Bind(&KeyedServiceBaseFactoryGetNodeName));
  base::WriteFile(dot_file, contents.c_str(), contents.size());
}
#endif  // NDEBUG
