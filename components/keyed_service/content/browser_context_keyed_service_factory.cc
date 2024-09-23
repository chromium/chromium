// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"

namespace {

// Wraps `factory` as a KeyedServiceFactory::TestingFactory.
base::OnceCallback<std::unique_ptr<KeyedService>(void*)> WrapFactory(
    BrowserContextKeyedServiceFactory::TestingFactory factory) {
  if (!factory) {
    return {};
  }

  return base::BindOnce(
      [](BrowserContextKeyedServiceFactory::TestingFactory factory,
         void* context) -> std::unique_ptr<KeyedService> {
        return std::move(factory).Run(
            static_cast<content::BrowserContext*>(context));
      },
      std::move(factory));
}

}  // namespace

void BrowserContextKeyedServiceFactory::SetTestingFactory(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  KeyedServiceFactory::SetTestingFactory(
      context, WrapFactory(std::move(testing_factory)));
}

KeyedService* BrowserContextKeyedServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  return KeyedServiceFactory::SetTestingFactoryAndUse(
      context, WrapFactory(std::move(testing_factory)));
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
BrowserContextKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(tsepez): fully deprecate the form below.
  return base::WrapUnique(BuildServiceInstanceFor(context));
}

KeyedService* BrowserContextKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Stub to prevent converted sub-classes from needing to implement this form.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<KeyedService>
BrowserContextKeyedServiceFactory::BuildServiceInstanceFor(
    void* context) const {
  return BuildServiceInstanceForBrowserContext(
      static_cast<content::BrowserContext*>(context));
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
