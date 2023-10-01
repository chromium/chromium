// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_factory.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_keyed_service.pbzero.h"

namespace {

base::flat_map<void*, int>& GetKeyedServicesCount() {
  // A static map to keep the count of currently active KeyedServices per
  // context.
  static base::NoDestructor<base::flat_map<void*, int>> keyed_services_count_;
  return *keyed_services_count_;
}

}  // namespace

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
  TRACE_EVENT("browser,startup", "KeyedServiceFactory::GetServiceForContext",
              [this](perfetto::EventContext ctx) {
                ctx.event()->set_chrome_keyed_service()->set_name(name());
              });
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
  // If `context` is already in `mapping_`, then something has gone wrong in
  // initializing services.
  // TODO(crbug.com/1487955): convert to CHECK
  DUMP_WILL_BE_CHECK(!base::Contains(mapping_, context));
  // Only count non-null services
  if (service)
    GetKeyedServicesCount()[context]++;
  auto iterator = mapping_.emplace(context, std::move(service)).first;
  return iterator->second.get();
}

void KeyedServiceFactory::Disassociate(void* context) {
  auto iterator = mapping_.find(context);
  if (iterator != mapping_.end()) {
    // if a service was null, it is not considered in the count.
    if (iterator->second && --GetKeyedServicesCount()[context] == 0)
      GetKeyedServicesCount().erase(context);
    mapping_.erase(iterator);
  }
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

bool KeyedServiceFactory::IsServiceCreated(void* context) const {
  auto it = mapping_.find(context);
  return it != mapping_.end() && it->second != nullptr;
}

// static
int KeyedServiceFactory::GetServicesCount(void* context) {
  auto it = GetKeyedServicesCount().find(context);
  return it != GetKeyedServicesCount().end() ? it->second : 0;
}
