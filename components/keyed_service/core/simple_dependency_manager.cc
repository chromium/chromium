// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/simple_dependency_manager.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "components/keyed_service/core/simple_factory_key.h"

#ifndef NDEBUG
#include "base/command_line.h"
#include "base/files/file_util.h"

namespace {

// Dumps dependency information about our simple keyed services
// into a dot file in the browser context directory.
const char kDumpSimpleDependencyGraphFlag[] = "dump-simple-graph";

}  // namespace
#endif  // NDEBUG

void SimpleDependencyManager::DestroyKeyedServices(SimpleFactoryKey* key) {
  DependencyManager::DestroyContextServices(key);
}

// static
SimpleDependencyManager* SimpleDependencyManager::GetInstance() {
  static base::NoDestructor<SimpleDependencyManager> factory;
  return factory.get();
}

void SimpleDependencyManager::RegisterProfilePrefsForServices(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  TRACE_EVENT0("browser",
               "SimpleDependencyManager::RegisterProfilePrefsForServices");
  RegisterPrefsForServices(pref_registry);
}

void SimpleDependencyManager::CreateServicesForTest(SimpleFactoryKey* key) {
  TRACE_EVENT0("browser", "SimpleDependencyManager::CreateServices");
  DependencyManager::CreateContextServices(key, true);
}

void SimpleDependencyManager::MarkContextLive(SimpleFactoryKey* key) {
  DependencyManager::MarkContextLive(key);
}

SimpleDependencyManager::SimpleDependencyManager() = default;

SimpleDependencyManager::~SimpleDependencyManager() = default;

#ifndef NDEBUG
void SimpleDependencyManager::DumpContextDependencies(void* context) const {
  // Whenever we try to build a destruction ordering, we should also dump a
  // dependency graph to "/path/to/context/context-dependencies.dot".
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDumpSimpleDependencyGraphFlag)) {
    base::FilePath dot_file =
        static_cast<const SimpleFactoryKey*>(context)->GetPath().AppendASCII(
            "simple-dependencies.dot");
    DumpDependenciesAsGraphviz("SimpleDependencyManager", dot_file);
  }
}
#endif  // NDEBUG
