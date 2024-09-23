// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_base_factory.h"

#include <type_traits>

#include "base/supports_user_data.h"
#include "base/trace_event/trace_event.h"
#include "components/keyed_service/core/dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

KeyedServiceBaseFactory::KeyedServiceBaseFactory(const char* service_name,
                                                 DependencyManager* manager,
                                                 Type type)
    : dependency_manager_(manager), service_name_(service_name), type_(type) {
  static_assert(
      std::is_same<decltype(KeyedServiceBaseFactory::service_name_),
                   const char*>::value,
      "This string is logged in plaintext via UMA trace events uploads, so "
      "must be static as a privacy requirement. See //third_party/perfetto/"
      "protos/perfetto/trace/track_event/chrome_keyed_service.proto. "
      "Contact tracing@chromium.org and base/metrics/OWNERS if this needs to "
      "change.");

  dependency_manager_->AddComponent(this);
}

KeyedServiceBaseFactory::~KeyedServiceBaseFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dependency_manager_->RemoveComponent(this);
}

void KeyedServiceBaseFactory::DependsOn(KeyedServiceBaseFactory* rhs) {
  CHECK_NE(rhs, this)
      << "A KeyedServiceBaseFactory instance must not depend on itself";

  // Each type can only depend on other services that are of the same type.
  if (rhs->type() != type_)
    return;

  dependency_manager_->AddEdge(rhs, this);
}

void KeyedServiceBaseFactory::AssertContextWasntDestroyed(void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dependency_manager_->AssertContextWasntDestroyed(context);
}

void KeyedServiceBaseFactory::MarkContextLive(void* context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dependency_manager_->MarkContextLive(context);
}

bool KeyedServiceBaseFactory::ServiceIsRequiredForContextInitialization()
    const {
  return false;
}

bool KeyedServiceBaseFactory::ServiceIsCreatedWithContext() const {
  return false;
}

bool KeyedServiceBaseFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

void KeyedServiceBaseFactory::ContextDestroyed(void* context) {
  // While object destruction can be customized in ways where the object is
  // only dereferenced, this still must run on the UI thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void KeyedServiceBaseFactory::CreateServiceNowForTesting(void* context) {
  CreateServiceNow(context);
}
