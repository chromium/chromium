// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/simple_keyed_service_factory.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

namespace {

// Wraps `factory` as a KeyedServiceFactory::TestingFactory.
base::OnceCallback<std::unique_ptr<KeyedService>(void*)> WrapFactory(
    SimpleKeyedServiceFactory::TestingFactory factory) {
  if (!factory) {
    return {};
  }

  return base::BindOnce(
      [](SimpleKeyedServiceFactory::TestingFactory factory,
         void* context) -> std::unique_ptr<KeyedService> {
        return std::move(factory).Run(static_cast<SimpleFactoryKey*>(context));
      },
      std::move(factory));
}

}  // namespace

void SimpleKeyedServiceFactory::SetTestingFactory(
    SimpleFactoryKey* key,
    TestingFactory testing_factory) {
  KeyedServiceFactory::SetTestingFactory(
      key, WrapFactory(std::move(testing_factory)));
}

KeyedService* SimpleKeyedServiceFactory::SetTestingFactoryAndUse(
    SimpleFactoryKey* key,
    TestingFactory testing_factory) {
  DCHECK(testing_factory);
  return KeyedServiceFactory::SetTestingFactoryAndUse(
      key, WrapFactory(std::move(testing_factory)));
}

SimpleKeyedServiceFactory::SimpleKeyedServiceFactory(
    const char* name,
    SimpleDependencyManager* manager)
    : KeyedServiceFactory(name, manager, SIMPLE) {}

SimpleKeyedServiceFactory::~SimpleKeyedServiceFactory() = default;

KeyedService* SimpleKeyedServiceFactory::GetServiceForKey(SimpleFactoryKey* key,
                                                          bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return KeyedServiceFactory::GetServiceForContext(key, create);
}

SimpleFactoryKey* SimpleKeyedServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  // Safe default for Incognito mode: no service.
  if (key->IsOffTheRecord())
    return nullptr;

  return key;
}

void SimpleKeyedServiceFactory::SimpleContextShutdown(SimpleFactoryKey* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  KeyedServiceFactory::ContextShutdown(key);
}

void SimpleKeyedServiceFactory::SimpleContextDestroyed(SimpleFactoryKey* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  KeyedServiceFactory::ContextDestroyed(key);
}

std::unique_ptr<KeyedService>
SimpleKeyedServiceFactory::BuildServiceInstanceFor(void* context) const {
  return BuildServiceInstanceFor(static_cast<SimpleFactoryKey*>(context));
}

void* SimpleKeyedServiceFactory::GetContextToUse(void* context) const {
  AssertContextWasntDestroyed(context);
  return GetKeyToUse(static_cast<SimpleFactoryKey*>(context));
}

bool SimpleKeyedServiceFactory::ServiceIsCreatedWithContext() const {
  return false;
}

void SimpleKeyedServiceFactory::ContextShutdown(void* context) {
  SimpleContextShutdown(static_cast<SimpleFactoryKey*>(context));
}

void SimpleKeyedServiceFactory::ContextDestroyed(void* context) {
  SimpleContextDestroyed(static_cast<SimpleFactoryKey*>(context));
}

void SimpleKeyedServiceFactory::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);
}

void SimpleKeyedServiceFactory::CreateServiceNow(void* context) {}
