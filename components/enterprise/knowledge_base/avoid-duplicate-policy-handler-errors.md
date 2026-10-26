---
name: enterprise/avoid-duplicate-policy-handler-errors
rubric: correctness
severity: should_fix
---

**Rule.** When a `ConfigurationPolicyHandler` or policy validation helper
delegates validation of a companion policy to another handler inside
`CheckPolicySettings()`, guard the companion check with
`if (errors && !errors->HasError(companion_policy_name))` so `chrome://policy`
does not display duplicate error messages. Additionally, do not use
`SimpleDeprecatingPolicyHandler` for unreleased (`future_on`) policies.

**Why.** `PolicyErrorMap` does not deduplicate errors when both a primary
handler and a companion handler validate the same dependent policy during
`CheckPolicySettings()`. Guarding the companion handler call with
`!errors->HasError(...)` avoids duplicate error entries in `chrome://policy`.
`SimpleDeprecatingPolicyHandler` is intended for migrating shipped policies and
causes confusing deprecation warnings when applied to `future_on` policies.

## Examples

Bad:

```cpp
bool CheckUserAffiliatedProxyPolicy(const policy::PolicyMap& policies,
                                    const std::string& policy_name,
                                    policy::PolicyErrorMap* errors) {
  // Unconditionally runs the companion handler, adding duplicate errors in
  // chrome://policy when the companion handler also runs in the handler list:
  policy::IntRangePolicyHandler enabled_for_all_users_handler(
      policy::key::kEnableProxyOverrideRulesForAllUsers,
      /*pref_path=*/nullptr, /*min=*/0, /*max=*/1, /*clamp=*/false);
  enabled_for_all_users_handler.CheckPolicySettings(policies, errors);
  return true;
}
```

Good:

```cpp
bool CheckUserAffiliatedProxyPolicy(const policy::PolicyMap& policies,
                                    const std::string& policy_name,
                                    policy::PolicyErrorMap* errors) {
  if (errors &&
      !errors->HasError(policy::key::kEnableProxyOverrideRulesForAllUsers)) {
    policy::IntRangePolicyHandler enabled_for_all_users_handler(
        policy::key::kEnableProxyOverrideRulesForAllUsers,
        /*pref_path=*/nullptr, /*min=*/0, /*max=*/1, /*clamp=*/false);
    enabled_for_all_users_handler.CheckPolicySettings(policies, errors);
  }

  const policy::PolicyMap::Entry* entry = policies.Get(policy_name);
  if (!entry) {
    return true;
  }

  if (entry->scope == policy::PolicyScope::POLICY_SCOPE_USER &&
      !policies.GetDeviceAffiliationIds().empty() &&
      !policies.IsUserAffiliated() &&
      !IsUnaffiliatedUserProxyPolicyAllowed(policies)) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_UNAFFILIATED_USER_ERROR);
    }
    return false;
  }
  return true;
}
```
