// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/simple_dependency_manager.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "components/keyed_service/core/simple_factory_key.h"

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

void SimpleDependencyManager::CreateServices(SimpleFactoryKey* key) {
  TRACE_EVENT0("browser", "SimpleDependencyManager::CreateServices");
  DependencyManager::CreateContextServices(key, false);
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
