// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/incognito/incognito_mode_policy_handler.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/incognito/incognito_mode_policy_handler_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class IncognitoModePolicyHandlerTest
    : public IncognitoModePolicyHandlerTestBase {
 public:
  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new IncognitoModePolicyHandler));
  }

 protected:
  base::ListValue GetUrlListPolicyValueWithEntries(size_t len) {
    base::ListValue list;
    for (size_t i = 0; i < len; ++i) {
      list.Append("http://example" + base::NumberToString(i) + ".com");
    }
    return list;
  }
};

TEST_F(IncognitoModePolicyHandlerTest, NoPolicySet) {
  ApplyPolicies();

  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlBlocklist, &value));
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlAllowlist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AvailabilityDisabled) {
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

TEST_F(IncognitoModePolicyHandlerTest, BlocklistSetAndAvailabilityDisabled) {
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

// Checks that if only the allowlist is set, the blocklist is set to "*".
TEST_F(IncognitoModePolicyHandlerTest, AllowlistSet) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  ApplyPolicies();
  VerifyAllowlistPref(default_allowlist_);
  VerifyBlocklistPref(base::ListValue().Append("*"));
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistSetWithBlocklistSet) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  ApplyPolicies();
  VerifyBlocklistPref(default_blocklist_);
  VerifyAllowlistPref(default_allowlist_);
}

// Checks that if allowlist is set and availability is disabled, the Incognito
// mode is enabled and blocklist is set to "*".
TEST_F(IncognitoModePolicyHandlerTest, AllowlistSetWithAvailabilityDisabled) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

// Checks that if allowlist is set, blocklist is set and availability is
// disabled, then Incognito mode is enabled and the blocklist is set to "*".
TEST_F(IncognitoModePolicyHandlerTest,
       AllowlistSetWithBlocklistAndAvailabilityDisabled) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kDisabled);
  ApplyPolicies();
  VerifyAllowlistPref(default_allowlist_);
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistSetWithAvailabilityForced) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kForced);
  ApplyPolicies();
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kForced);
}

// Checks that if allowlist is set, blocklist is set and availability is
// forced, then Incognito mode is forced and the blocklist is not changed.
TEST_F(IncognitoModePolicyHandlerTest,
       AllowlistSetWithBlocklistAndAvailabilityForced) {
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  SetIncognitoModeAvailability(policy::IncognitoModeAvailability::kForced);
  ApplyPolicies();
  VerifyBlocklistPref(default_blocklist_);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kForced);
}

TEST_F(IncognitoModePolicyHandlerTest, AvailabilityInvalidType) {
  policies_.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value("invalid"),
                nullptr);

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AvailabilityOutOfRange) {
  policies_.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(
                    policy::IncognitoModeAvailability::kNumTypes)),
                nullptr);

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistInvalidType) {
  policies_.Set(key::kIncognitoModeUrlAllowlist, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(123),
                nullptr);

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlAllowlist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, BlocklistInvalidType) {
  policies_.Set(key::kIncognitoModeUrlBlocklist, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(123),
                nullptr);

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlBlocklist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistEmpty) {
  SetIncognitoModeUrlAllowlist(base::ListValue());
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlAllowlist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, BlocklistEmpty) {
  SetIncognitoModeUrlBlocklist(base::ListValue());
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlBlocklist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, AllowlistWrongElementType) {
  base::ListValue in;
  in.Append(false);
  SetIncognitoModeUrlAllowlist(std::move(in));

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  ApplyPolicies();

  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlAllowlist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, BlocklistWrongElementType) {
  base::ListValue in;
  in.Append(false);
  SetIncognitoModeUrlBlocklist(std::move(in));

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  ApplyPolicies();

  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeUrlBlocklist, &value));
}

TEST_F(IncognitoModePolicyHandlerTest, MaxFiltersLimitOK) {
  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::ListValue urls =
      GetUrlListPolicyValueWithEntries(max_filters_per_policy);

  SetIncognitoModeUrlAllowlist(urls.Clone());

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(0U, errors.size());

  ApplyPolicies();
  VerifyAllowlistPref(urls);
}

TEST_F(IncognitoModePolicyHandlerTest, MaxFiltersLimitExceeded) {
  size_t max_filters_per_policy = policy::kMaxUrlFiltersPerPolicy;
  base::ListValue urls =
      GetUrlListPolicyValueWithEntries(max_filters_per_policy + 1);

  SetIncognitoModeUrlAllowlist(urls.Clone());

  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(1U, errors.size());

  auto error_str = errors.GetErrorMessages(key::kIncognitoModeUrlAllowlist);
  auto expected_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(max_filters_per_policy));
  EXPECT_TRUE(error_str.find(expected_str) != std::u16string::npos);

  ApplyPolicies();

  // IncognitoModePolicyHandler truncates the list.
  base::ListValue expected_urls =
      GetUrlListPolicyValueWithEntries(max_filters_per_policy);
  VerifyAllowlistPref(expected_urls);
}

TEST_F(IncognitoModePolicyHandlerTest, FilterInvalidUrls) {
  base::ListValue in;
  in.Append("http://valid.com");
  in.Append("wsgi:///invalid.com");
  SetIncognitoModeUrlAllowlist(std::move(in));
  ApplyPolicies();

  base::ListValue expected;
  expected.Append("http://valid.com");
  VerifyAllowlistPref(expected);
}

TEST_F(IncognitoModePolicyHandlerTest, ValidatePolicy) {
  IncognitoModePolicyHandler handler;
  PolicyErrorMap errors;

  auto check_url = [&](const std::string& url) {
    errors.Clear();
    policies_.Clear();
    base::ListValue list;
    list.Append(url);
    SetIncognitoModeUrlAllowlist(std::move(list));
    return handler.CheckPolicySettings(policies_, &errors);
  };

  EXPECT_TRUE(check_url("http://*"));
  EXPECT_EQ(0U, errors.size());

  EXPECT_TRUE(check_url("ws://example.org/component.js"));
  EXPECT_EQ(0U, errors.size());

  EXPECT_TRUE(check_url("wsgi:///rancom,org/"));
  EXPECT_EQ(1U, errors.size());

  EXPECT_TRUE(check_url("127.0.0.1:65535"));
  EXPECT_EQ(0U, errors.size());

  EXPECT_TRUE(check_url("127.0.0.1:65536"));
  EXPECT_EQ(1U, errors.size());

  EXPECT_TRUE(check_url("*"));
  EXPECT_EQ(0U, errors.size());

  EXPECT_TRUE(check_url("*.developers.com"));
  EXPECT_EQ(1U, errors.size());
}

}  // namespace policy
