// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/refcounted_keyed_service_factory.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"

RefcountedKeyedServiceFactory::RefcountedKeyedServiceFactory(
    const char* name,
    DependencyManager* manager,
    Type type)
    : KeyedServiceBaseFactory(name, manager, type) {}

RefcountedKeyedServiceFactory::~RefcountedKeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

void RefcountedKeyedServiceFactory::SetTestingFactory(
    void* context,
    TestingFactory testing_factory) {
  // Ensure that |context| is not marked as stale (e.g., due to it aliasing an
  // instance that was destroyed in an earlier test) in order to avoid accesses
  // to |context| in |ContextShutdown| from causing
  // |AssertBrowserContextWasntDestroyed| to raise an error.
  MarkContextLive(context);

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a context and then change the
  // testing service mid-test.
  ContextShutdown(context);
  ContextDestroyed(context);

  testing_factories_.emplace(context, std::move(testing_factory));
}

scoped_refptr<RefcountedKeyedService>
RefcountedKeyedServiceFactory::SetTestingFactoryAndUse(
    void* context,
    TestingFactory testing_factory) {
  DCHECK(testing_factory);
  SetTestingFactory(context, std::move(testing_factory));
  return GetServiceForContext(context, true);
}

scoped_refptr<RefcountedKeyedService>
RefcountedKeyedServiceFactory::GetServiceForContext(void* context,
                                                    bool create) {
  context = GetContextToUse(context);
  if (!context)
    return nullptr;

  // NOTE: If you modify any of the logic below, make sure to update the
  // non-refcounted version in context_keyed_service_factory.cc!
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end())
    return iterator->second;

  // Object not found.
  if (!create)
    return nullptr;  // And we're forbidden from creating one.

  // Create new object.
  // Check to see if we have a per-BrowserContext testing factory that we should
  // use instead of default behavior.
  scoped_refptr<RefcountedKeyedService> service;
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

scoped_refptr<RefcountedKeyedService> RefcountedKeyedServiceFactory::Associate(
    void* context,
    scoped_refptr<RefcountedKeyedService> service) {
  DCHECK(!base::Contains(mapping_, context));
  auto iterator = mapping_.emplace(context, std::move(service)).first;
  return iterator->second;
}

void RefcountedKeyedServiceFactory::Disassociate(void* context) {
  // We "merely" drop our reference to the service. Hopefully this will cause
  // the service to be destroyed. If not, oh well.
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end())
    mapping_.erase(iterator);
}

void RefcountedKeyedServiceFactory::ContextShutdown(void* context) {
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end() && iterator->second.get())
    iterator->second->ShutdownOnUIThread();
}

void RefcountedKeyedServiceFactory::ContextDestroyed(void* context) {
  Disassociate(context);

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  testing_factories_.erase(context);

  KeyedServiceBaseFactory::ContextDestroyed(context);
}

void RefcountedKeyedServiceFactory::SetEmptyTestingFactory(void* context) {
  SetTestingFactory(context, TestingFactory());
}

bool RefcountedKeyedServiceFactory::HasTestingFactory(void* context) {
  return base::Contains(testing_factories_, context);
}

void RefcountedKeyedServiceFactory::CreateServiceNow(void* context) {
  GetServiceForContext(context, true);
}
