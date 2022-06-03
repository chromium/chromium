// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "content/public/browser/browser_context.h"

void RefcountedBrowserContextKeyedServiceFactory::SetTestingFactory(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  RefcountedKeyedServiceFactory::TestingFactory wrapped_factory;
  if (testing_factory) {
    wrapped_factory = base::BindRepeating(
        [](const TestingFactory& testing_factory, void* context) {
          return testing_factory.Run(
              static_cast<content::BrowserContext*>(context));
        },
        std::move(testing_factory));
  }
  RefcountedKeyedServiceFactory::SetTestingFactory(context,
                                                   std::move(wrapped_factory));
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserContextKeyedServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  DCHECK(testing_factory);
  return RefcountedKeyedServiceFactory::SetTestingFactoryAndUse(
      context,
      base::BindRepeating(
          [](const TestingFactory& testing_factory, void* context) {
            return testing_factory.Run(
                static_cast<content::BrowserContext*>(context));
          },
          std::move(testing_factory)));
}

RefcountedBrowserContextKeyedServiceFactory::
    RefcountedBrowserContextKeyedServiceFactory(
        const char* name,
        BrowserContextDependencyManager* manager)
    : RefcountedKeyedServiceFactory(name, manager, BROWSER_CONTEXT) {}

RefcountedBrowserContextKeyedServiceFactory::
    ~RefcountedBrowserContextKeyedServiceFactory() {
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserContextKeyedServiceFactory::GetServiceForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  return RefcountedKeyedServiceFactory::GetServiceForContext(context, create);
}

content::BrowserContext*
RefcountedBrowserContextKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Safe default for Incognito mode: no service.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}

bool RefcountedBrowserContextKeyedServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return KeyedServiceBaseFactory::ServiceIsCreatedWithContext();
}

bool RefcountedBrowserContextKeyedServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return KeyedServiceBaseFactory::ServiceIsNULLWhileTesting();
}

void RefcountedBrowserContextKeyedServiceFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  RefcountedKeyedServiceFactory::ContextShutdown(context);
}

void RefcountedBrowserContextKeyedServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  RefcountedKeyedServiceFactory::ContextDestroyed(context);
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserContextKeyedServiceFactory::BuildServiceInstanceFor(
    void* context) const {
  return BuildServiceInstanceFor(
      static_cast<content::BrowserContext*>(context));
}

bool RefcountedBrowserContextKeyedServiceFactory::IsOffTheRecord(
    void* context) const {
  return static_cast<content::BrowserContext*>(context)->IsOffTheRecord();
}

void* RefcountedBrowserContextKeyedServiceFactory::GetContextToUse(
    void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AssertContextWasntDestroyed(context);
  return GetBrowserContextToUse(static_cast<content::BrowserContext*>(context));
}

bool RefcountedBrowserContextKeyedServiceFactory::ServiceIsCreatedWithContext()
    const {
  return ServiceIsCreatedWithBrowserContext();
}

void RefcountedBrowserContextKeyedServiceFactory::ContextShutdown(
    void* context) {
  BrowserContextShutdown(static_cast<content::BrowserContext*>(context));
}

void RefcountedBrowserContextKeyedServiceFactory::ContextDestroyed(
    void* context) {
  BrowserContextDestroyed(static_cast<content::BrowserContext*>(context));
}

void RefcountedBrowserContextKeyedServiceFactory::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);
}
