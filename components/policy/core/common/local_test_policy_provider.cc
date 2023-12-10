// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/local_test_policy_provider.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

// static
std::unique_ptr<LocalTestPolicyProvider>
LocalTestPolicyProvider::CreateIfAllowed(version_info::Channel channel) {
  if (utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr, channel)) {
    return base::WrapUnique(new LocalTestPolicyProvider());
  }

  return nullptr;
}

LocalTestPolicyProvider::~LocalTestPolicyProvider() = default;

void LocalTestPolicyProvider::LoadJsonPolicies(
    const std::string& json_policies_string) {
  loader_.SetPolicyListJson(json_policies_string);
  RefreshPolicies(PolicyFetchReason::kUnspecified);
}

void LocalTestPolicyProvider::SetUserAffiliated(bool affiliated) {
  loader_.SetUserAffiliated(affiliated);
}

const std::string& LocalTestPolicyProvider::GetPolicies() const {
  return loader_.policies();
}

void LocalTestPolicyProvider::ClearPolicies() {
  loader_.ClearPolicies();
  RefreshPolicies(PolicyFetchReason::kUnspecified);
}

void LocalTestPolicyProvider::RefreshPolicies(PolicyFetchReason reason) {
  PolicyBundle bundle = loader_.Load();
  first_policies_loaded_ = true;
  UpdatePolicy(std::move(bundle));
}

bool LocalTestPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return first_policies_loaded_;
}

// static
void LocalTestPolicyProvider::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      policy::policy_prefs::kLocalTestPoliciesForNextStartup, std::string());
}

LocalTestPolicyProvider::LocalTestPolicyProvider() {
  set_active(false);
  RefreshPolicies(PolicyFetchReason::kUnspecified);
}

}  // namespace policy
