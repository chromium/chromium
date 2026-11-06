// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_policy_utils.h"

#include <string>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/enterprise/net/core/features.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_net {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
namespace {

bool IsAffiliationEnforcedOnPlatformManagedDevices() {
  // Policy handlers can run before `base::FeatureList` is initialized while
  // Local State is created at startup. Use the feature's default state in that
  // case instead of accessing the feature early; profile-scoped policies are
  // re-evaluated once `FeatureList` is initialized, and proxy override rules
  // are also checked by `proxy_config::ProxyOverrideRulesAllowed()` before
  // being applied.
  if (!base::FeatureList::GetInstance()) {
    return kEnforceProxyAffiliationOnPlatformManagedDevices.GetDefaultState() ==
           base::FEATURE_ENABLED_BY_DEFAULT;
  }
  return base::FeatureList::IsEnabled(
      kEnforceProxyAffiliationOnPlatformManagedDevices);
}

}  // namespace

bool IsDeviceManaged(const policy::PolicyMap& policies) {
  if (!policies.GetDeviceAffiliationIds().empty()) {
    return true;
  }

  // When disabled, `IsDeviceManaged()` reduces to
  // `!policies.GetDeviceAffiliationIds().empty()`, matching the check prior to
  // https://crrev.com/c/8618781 in both `CheckUserAffiliatedProxyPolicy()` and
  // `proxy_config::AreUserProxyOverrideRulesAllowed()`.
  if (!IsAffiliationEnforcedOnPlatformManagedDevices()) {
    return false;
  }

  // Note: On Android, `AndroidManagementStatusProvider` caches the EMM
  // management state in `local_state` (`kEnterpriseMDMManagementAndroid`) and
  // refreshes it asynchronously via JNI on startup. On the very first cold run
  // before the cache is populated, this may return false until the next policy
  // refresh or browser restart.
  //
  // On iOS, `DeviceManagementStatusProvider` is uncached (`RequiresCache() ==
  // false`) and synchronously checks `NSUserDefaults` for
  // `kPolicyLoaderIOSConfigurationKey` ("com.apple.configuration.managed"),
  // returning `EnterpriseManagementAuthority::CLOUD`
  // (`ManagementAuthorityTrustworthiness::TRUSTED`) when MDM AppConfig is
  // present—so unlike Android, there is no cold-start `local_state` cache race.
  // MDM policies loaded via `PolicyLoaderIOS` are set at
  // `POLICY_SCOPE_MACHINE`, so MDM-managed devices without CBCM device
  // affiliation IDs enforce affiliation on `POLICY_SCOPE_USER` cloud policies
  // unless `EnableProxyOverrideRulesForAllUsers` is set to `1` at
  // `POLICY_SCOPE_MACHINE` (via MDM or CBCM).
  return policy::PlatformManagementService::GetInstance()
             ->GetManagementAuthorityTrustworthiness() >=
         policy::ManagementAuthorityTrustworthiness::TRUSTED;
}

bool IsUnaffiliatedUserProxyPolicyAllowed(const policy::PolicyMap& policies) {
  const policy::PolicyMap::Entry* entry =
      policies.Get(policy::key::kEnableProxyOverrideRulesForAllUsers);
  if (!entry || entry->scope != policy::POLICY_SCOPE_MACHINE) {
    return false;
  }
  const base::Value* value = entry->value(base::Value::Type::INTEGER);
  return value && value->GetInt() == 1;
}

bool CheckUserAffiliatedProxyPolicy(const policy::PolicyMap& policies,
                                    const std::string& policy_name,
                                    policy::PolicyErrorMap* errors) {
  if (errors &&
      !errors->HasError(policy::key::kEnableProxyOverrideRulesForAllUsers)) {
    policy::IntRangePolicyHandler enabled_for_all_users_handler(
        policy::key::kEnableProxyOverrideRulesForAllUsers,
        /*pref_path=*/nullptr,
        /*min=*/0,
        /*max=*/1,
        /*clamp=*/false);
    enabled_for_all_users_handler.CheckPolicySettings(policies, errors);
  }

  const policy::PolicyMap::Entry* entry = policies.Get(policy_name);
  if (!entry) {
    return true;
  }

  if (entry->scope == policy::PolicyScope::POLICY_SCOPE_USER &&
      !policies.IsUserAffiliated() &&
      !IsUnaffiliatedUserProxyPolicyAllowed(policies) &&
      IsDeviceManaged(policies)) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_UNAFFILIATED_USER_ERROR);
    }
    return false;
  }
  return true;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace enterprise_net
