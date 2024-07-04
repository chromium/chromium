// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web/public/browser_state.h"

namespace {

// Wraps `factory` as a KeyedServiceFactory::TestingFactory.
base::OnceCallback<scoped_refptr<RefcountedKeyedService>(void*)> WrapFactory(
    RefcountedBrowserStateKeyedServiceFactory::TestingFactory factory) {
  if (!factory) {
    return {};
  }

  return base::BindOnce(
      [](RefcountedBrowserStateKeyedServiceFactory::TestingFactory factory,
         void* context) -> scoped_refptr<RefcountedKeyedService> {
        return std::move(factory).Run(static_cast<web::BrowserState*>(context));
      },
      std::move(factory));
}

}  // namespace

void RefcountedBrowserStateKeyedServiceFactory::SetTestingFactory(
    web::BrowserState* context,
    TestingFactory testing_factory) {
  RefcountedKeyedServiceFactory::SetTestingFactory(
      context, WrapFactory(std::move(testing_factory)));
}

RefcountedBrowserStateKeyedServiceFactory::
    RefcountedBrowserStateKeyedServiceFactory(
        const char* name,
        BrowserStateDependencyManager* manager)
    : RefcountedKeyedServiceFactory(name, manager, BROWSER_STATE) {}

RefcountedBrowserStateKeyedServiceFactory::
    ~RefcountedBrowserStateKeyedServiceFactory() {
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserStateKeyedServiceFactory::GetServiceForBrowserState(
    web::BrowserState* context,
    bool create) {
  return RefcountedKeyedServiceFactory::GetServiceForContext(context, create);
}

web::BrowserState*
RefcountedBrowserStateKeyedServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Safe default for Incognito mode: no service.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}

bool RefcountedBrowserStateKeyedServiceFactory::
    ServiceIsCreatedWithBrowserState() const {
  return KeyedServiceBaseFactory::ServiceIsCreatedWithContext();
}

bool RefcountedBrowserStateKeyedServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return KeyedServiceBaseFactory::ServiceIsNULLWhileTesting();
}

void RefcountedBrowserStateKeyedServiceFactory::BrowserStateShutdown(
    web::BrowserState* context) {
  RefcountedKeyedServiceFactory::ContextShutdown(context);
}

void RefcountedBrowserStateKeyedServiceFactory::BrowserStateDestroyed(
    web::BrowserState* context) {
  RefcountedKeyedServiceFactory::ContextDestroyed(context);
}

scoped_refptr<RefcountedKeyedService>
RefcountedBrowserStateKeyedServiceFactory::BuildServiceInstanceFor(
    void* context) const {
  return BuildServiceInstanceFor(static_cast<web::BrowserState*>(context));
}

void* RefcountedBrowserStateKeyedServiceFactory::GetContextToUse(
    void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AssertContextWasntDestroyed(context);
  return GetBrowserStateToUse(static_cast<web::BrowserState*>(context));
}

bool RefcountedBrowserStateKeyedServiceFactory::ServiceIsCreatedWithContext()
    const {
  return ServiceIsCreatedWithBrowserState();
}

void RefcountedBrowserStateKeyedServiceFactory::ContextShutdown(void* context) {
  BrowserStateShutdown(static_cast<web::BrowserState*>(context));
}

void RefcountedBrowserStateKeyedServiceFactory::ContextDestroyed(
    void* context) {
  BrowserStateDestroyed(static_cast<web::BrowserState*>(context));
}

void RefcountedBrowserStateKeyedServiceFactory::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterBrowserStatePrefs(registry);
}
