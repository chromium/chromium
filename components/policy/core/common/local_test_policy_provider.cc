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
#include "components/prefs/pref_registry_simple.h"

namespace policy {

// static
std::unique_ptr<LocalTestPolicyProvider>
LocalTestPolicyProvider::CreateIfAllowed(version_info::Channel channel) {
  if (IsAllowed(channel)) {
    return base::WrapUnique(new LocalTestPolicyProvider());
  }

  return nullptr;
}

bool LocalTestPolicyProvider::IsAllowed(version_info::Channel channel) {
  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEFAULT) {
    return true;
  }

#if BUILDFLAG(IS_IOS)
  if (channel == version_info::Channel::BETA) {
    return true;
  }
#endif

  return false;
}

LocalTestPolicyProvider::~LocalTestPolicyProvider() = default;

void LocalTestPolicyProvider::LoadJsonPolicies(
    const std::string& json_policies_string) {
  loader_.SetPolicyListJson(json_policies_string);
  RefreshPolicies();
}

void LocalTestPolicyProvider::SetUserAffiliated(bool affiliated) {
  loader_.SetUserAffiliated(affiliated);
}

void LocalTestPolicyProvider::ClearPolicies() {
  loader_.ClearPolicies();
  RefreshPolicies();
}

void LocalTestPolicyProvider::RefreshPolicies() {
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
  RefreshPolicies();
}

}  // namespace policy
