// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_INCOGNITO_INCOGNITO_MODE_POLICY_HANDLER_TEST_H_
#define COMPONENTS_POLICY_CORE_BROWSER_INCOGNITO_INCOGNITO_MODE_POLICY_HANDLER_TEST_H_

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"

namespace policy {

// Tests Incognito mode availability preference setting.
class IncognitoModePolicyHandlerTestBase
    : public ConfigurationPolicyPrefStoreTest {
 public:
  IncognitoModePolicyHandlerTestBase();

 protected:
  enum ObsoleteIncognitoEnabledValue {
    INCOGNITO_ENABLED_UNKNOWN,
    INCOGNITO_ENABLED_TRUE,
    INCOGNITO_ENABLED_FALSE
  };

  PolicyMap policies_;
  base::ListValue default_blocklist_;
  base::ListValue default_allowlist_;

  void SetIncognitoModeAvailability(
      policy::IncognitoModeAvailability availability);
  void SetIncognitoModeUrlAllowlist(base::ListValue allowlist);
  void SetIncognitoModeUrlBlocklist(base::ListValue blocklist);
  void ApplyPolicies();
  void VerifyAvailabilityPref(policy::IncognitoModeAvailability availability);
  void VerifyBlocklistPref(const base::ListValue& expected_blocklist);
  void VerifyAllowlistPref(const base::ListValue& expected_allowlist);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_INCOGNITO_INCOGNITO_MODE_POLICY_HANDLER_TEST_H_
