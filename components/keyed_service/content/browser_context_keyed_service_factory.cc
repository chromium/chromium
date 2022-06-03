// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"

void BrowserContextKeyedServiceFactory::SetTestingFactory(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  KeyedServiceFactory::TestingFactory wrapped_factory;
  if (testing_factory) {
    wrapped_factory = base::BindRepeating(
        [](const TestingFactory& testing_factory, void* context) {
          return testing_factory.Run(
              static_cast<content::BrowserContext*>(context));
        },
        std::move(testing_factory));
  }
  KeyedServiceFactory::SetTestingFactory(context, std::move(wrapped_factory));
}

KeyedService* BrowserContextKeyedServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  DCHECK(testing_factory);
  return KeyedServiceFactory::SetTestingFactoryAndUse(
      context,
      base::BindRepeating(
          [](const TestingFactory& testing_factory, void* context) {
            return testing_factory.Run(
                static_cast<content::BrowserContext*>(context));
          },
          std::move(testing_factory)));
}

BrowserContextKeyedServiceFactory::BrowserContextKeyedServiceFactory(
    const char* name,
    BrowserContextDependencyManager* manager)
    : KeyedServiceFactory(name, manager, BROWSER_CONTEXT) {}

BrowserContextKeyedServiceFactory::~BrowserContextKeyedServiceFactory() {
}

KeyedService* BrowserContextKeyedServiceFactory::GetServiceForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  return KeyedServiceFactory::GetServiceForContext(context, create);
}

content::BrowserContext*
BrowserContextKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Safe default for Incognito mode: no service.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}

bool BrowserContextKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return KeyedServiceBaseFactory::ServiceIsCreatedWithContext();
}

bool BrowserContextKeyedServiceFactory::ServiceIsNULLWhileTesting() const {
  return KeyedServiceBaseFactory::ServiceIsNULLWhileTesting();
}

void BrowserContextKeyedServiceFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  KeyedServiceFactory::ContextShutdown(context);
}

void BrowserContextKeyedServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  KeyedServiceFactory::ContextDestroyed(context);
}

std::unique_ptr<KeyedService>
BrowserContextKeyedServiceFactory::BuildServiceInstanceFor(
    void* context) const {
  // TODO(isherman): The wrapped BuildServiceInstanceFor() should return a
  // scoped_ptr as well.
  return base::WrapUnique(
      BuildServiceInstanceFor(static_cast<content::BrowserContext*>(context)));
}

bool BrowserContextKeyedServiceFactory::IsOffTheRecord(void* context) const {
  return static_cast<content::BrowserContext*>(context)->IsOffTheRecord();
}

void* BrowserContextKeyedServiceFactory::GetContextToUse(void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AssertContextWasntDestroyed(context);
  return GetBrowserContextToUse(static_cast<content::BrowserContext*>(context));
}

bool BrowserContextKeyedServiceFactory::ServiceIsCreatedWithContext() const {
  return ServiceIsCreatedWithBrowserContext();
}

void BrowserContextKeyedServiceFactory::ContextShutdown(void* context) {
  BrowserContextShutdown(static_cast<content::BrowserContext*>(context));
}

void BrowserContextKeyedServiceFactory::ContextDestroyed(void* context) {
  BrowserContextDestroyed(static_cast<content::BrowserContext*>(context));
}

void BrowserContextKeyedServiceFactory::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);
}

void BrowserContextKeyedServiceFactory::CreateServiceNow(void* context) {
  KeyedServiceFactory::GetServiceForContext(context, true);
}
