// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web/public/browser_state.h"

namespace {

// Wraps `factory` as a KeyedServiceFactory::TestingFactory.
base::OnceCallback<std::unique_ptr<KeyedService>(void*)> WrapFactory(
    BrowserStateKeyedServiceFactory::TestingFactory factory) {
  if (!factory) {
    return {};
  }

  return base::BindOnce(
      [](BrowserStateKeyedServiceFactory::TestingFactory factory,
         void* context) -> std::unique_ptr<KeyedService> {
        return std::move(factory).Run(static_cast<web::BrowserState*>(context));
      },
      std::move(factory));
}

}  // namespace

void BrowserStateKeyedServiceFactory::SetTestingFactory(
    web::BrowserState* context,
    TestingFactory testing_factory) {
  KeyedServiceFactory::SetTestingFactory(
      context, WrapFactory(std::move(testing_factory)));
}

BrowserStateKeyedServiceFactory::BrowserStateKeyedServiceFactory(
    const char* name,
    BrowserStateDependencyManager* manager)
    : KeyedServiceFactory(name, manager, BROWSER_STATE) {}

BrowserStateKeyedServiceFactory::~BrowserStateKeyedServiceFactory() {
}

KeyedService* BrowserStateKeyedServiceFactory::GetServiceForBrowserState(
    web::BrowserState* context,
    bool create) {
  return KeyedServiceFactory::GetServiceForContext(context, create);
}

web::BrowserState* BrowserStateKeyedServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Safe default for Incognito mode: no service.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}

bool BrowserStateKeyedServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return KeyedServiceBaseFactory::ServiceIsCreatedWithContext();
}

bool BrowserStateKeyedServiceFactory::ServiceIsNULLWhileTesting() const {
  return KeyedServiceBaseFactory::ServiceIsNULLWhileTesting();
}

void BrowserStateKeyedServiceFactory::BrowserStateShutdown(
    web::BrowserState* context) {
  KeyedServiceFactory::ContextShutdown(context);
}

void BrowserStateKeyedServiceFactory::BrowserStateDestroyed(
    web::BrowserState* context) {
  KeyedServiceFactory::ContextDestroyed(context);
}

std::unique_ptr<KeyedService>
BrowserStateKeyedServiceFactory::BuildServiceInstanceFor(void* context) const {
  return BuildServiceInstanceFor(static_cast<web::BrowserState*>(context));
}

void* BrowserStateKeyedServiceFactory::GetContextToUse(void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AssertContextWasntDestroyed(context);
  return GetBrowserStateToUse(static_cast<web::BrowserState*>(context));
}

bool BrowserStateKeyedServiceFactory::ServiceIsCreatedWithContext() const {
  return ServiceIsCreatedWithBrowserState();
}

void BrowserStateKeyedServiceFactory::ContextShutdown(void* context) {
  BrowserStateShutdown(static_cast<web::BrowserState*>(context));
}

void BrowserStateKeyedServiceFactory::ContextDestroyed(void* context) {
  BrowserStateDestroyed(static_cast<web::BrowserState*>(context));
}

void BrowserStateKeyedServiceFactory::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterBrowserStatePrefs(registry);
}

void BrowserStateKeyedServiceFactory::CreateServiceNow(void* context) {
  KeyedServiceFactory::GetServiceForContext(context, true);
}
