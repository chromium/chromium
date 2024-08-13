// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"

#include <string.h>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "enterprise_policy_registry.h"

namespace optimization_guide {

EnterprisePolicyPref::EnterprisePolicyPref(const char* name) : name_(name) {}

model_execution::prefs::ModelExecutionEnterprisePolicyValue
EnterprisePolicyPref::GetValue(const PrefService* pref_service) const {
  CHECK(pref_service);
  return static_cast<
      model_execution::prefs::ModelExecutionEnterprisePolicyValue>(
      pref_service->GetInteger(name_));
}

EnterprisePolicyRegistry::EnterprisePolicyRegistry() = default;
EnterprisePolicyRegistry::~EnterprisePolicyRegistry() = default;

EnterprisePolicyRegistry& EnterprisePolicyRegistry::GetInstance() {
  static base::NoDestructor<EnterprisePolicyRegistry> registry;
  return *registry;
}

EnterprisePolicyPref EnterprisePolicyRegistry::Register(const char* name) {
  // We shouldn't be registering new policies after the prefs have been
  // registered in the pref service.
  CHECK(!immutable_);
  for (const EnterprisePolicyPref& policy : enterprise_policies_) {
    // Make sure there isn't already an enterprise policy registered with that
    // name.
    CHECK(strcmp(policy.name(), name) != 0);
  }
  enterprise_policies_.emplace_back(name);
  return EnterprisePolicyPref(name);
}

void EnterprisePolicyRegistry::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  for (const EnterprisePolicyPref& policy : enterprise_policies_) {
    registry->RegisterIntegerPref(
        policy.name(),
        static_cast<int>(model_execution::prefs::
                             ModelExecutionEnterprisePolicyValue::kAllow),
        PrefRegistry::LOSSY_PREF);
  }
  // From that point on, it's too late to modify the registry as the prefs
  // won't get registered.
  immutable_ = true;
}

void EnterprisePolicyRegistry::ClearForTesting() {
  enterprise_policies_.clear();
  immutable_ = false;
}

}  // namespace optimization_guide
