// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_factory.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

KeyedServiceFactory::KeyedServiceFactory(const char* name,
                                         DependencyManager* manager,
                                         Type type)
    : KeyedServiceBaseFactory(name, manager, type) {}

KeyedServiceFactory::~KeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

void KeyedServiceFactory::SetTestingFactory(void* context,
                                            TestingFactory testing_factory) {
  // Ensure that |context| is not marked as stale (e.g., due to it aliasing an
  // instance that was destroyed in an earlier test) in order to avoid accesses
  // to |context| in |BrowserContextShutdown| from causing
  // |AssertBrowserContextWasntDestroyed| to raise an error.
  MarkContextLive(context);

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a context and then change the
  // testing service mid-test.
  ContextShutdown(context);
  ContextDestroyed(context);

  testing_factories_.emplace(context, std::move(testing_factory));
}

KeyedService* KeyedServiceFactory::SetTestingFactoryAndUse(
    void* context,
    TestingFactory testing_factory) {
  DCHECK(testing_factory);
  SetTestingFactory(context, std::move(testing_factory));
  return GetServiceForContext(context, true);
}

KeyedService* KeyedServiceFactory::GetServiceForContext(void* context,
                                                        bool create) {
  TRACE_EVENT1("browser,startup", "KeyedServiceFactory::GetServiceForContext",
               "service_name", name());
  context = GetContextToUse(context);
  if (!context)
    return nullptr;

  // NOTE: If you modify any of the logic below, make sure to update the
  // refcounted version in refcounted_context_keyed_service_factory.cc!
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end())
    return iterator->second.get();

  // Object not found.
  if (!create)
    return nullptr;  // And we're forbidden from creating one.

  // Create new object.
  // Check to see if we have a per-context testing factory that we should use
  // instead of default behavior.
  std::unique_ptr<KeyedService> service;
  auto factory_iterator = testing_factories_.find(context);
  if (factory_iterator != testing_factories_.end()) {
    if (factory_iterator->second) {
      service = factory_iterator->second.Run(context);
    }
  } else {
    service = BuildServiceInstanceFor(context);
  }

  return Associate(context, std::move(service));
}

KeyedService* KeyedServiceFactory::Associate(
    void* context,
    std::unique_ptr<KeyedService> service) {
  DCHECK(!base::Contains(mapping_, context));
  auto iterator = mapping_.emplace(context, std::move(service)).first;
  return iterator->second.get();
}

void KeyedServiceFactory::Disassociate(void* context) {
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end())
    mapping_.erase(iterator);
}

void KeyedServiceFactory::ContextShutdown(void* context) {
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end() && iterator->second)
    iterator->second->Shutdown();
}

void KeyedServiceFactory::ContextDestroyed(void* context) {
  Disassociate(context);

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  testing_factories_.erase(context);

  KeyedServiceBaseFactory::ContextDestroyed(context);
}

void KeyedServiceFactory::SetEmptyTestingFactory(void* context) {
  SetTestingFactory(context, TestingFactory());
}

bool KeyedServiceFactory::HasTestingFactory(void* context) {
  return base::Contains(testing_factories_, context);
}
