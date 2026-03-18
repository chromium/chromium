// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler_list.h"

#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/android_buildflags.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

const char kPolicyName[] = "PolicyName";
const char kPolicyName2[] = "PolicyName2";
const int kPolicyValue = 12;

class StubPolicyHandler : public ConfigurationPolicyHandler {
 public:
  explicit StubPolicyHandler(const std::string& policy_name)
      : policy_name_(policy_name) {}
  ~StubPolicyHandler() override = default;

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override {
    // It's safe to use `GetValueUnsafe()` as multiple policy types are handled.
    return policies.GetValueUnsafe(policy_name_);
  }

 protected:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    prefs->SetInteger(
        policy_name_,
        policies.GetValue(policy_name_, base::Value::Type::INTEGER)->GetInt());
  }

 private:
  std::string policy_name_;
};

}  // namespace

class ConfigurationPolicyHandlerListTest : public ::testing::Test {
 public:
  void SetUp() override { CreateHandlerList(); }

  void AddSimplePolicy() {
    AddPolicy(kPolicyName, /*is_cloud=*/true, base::Value(kPolicyValue));
  }

  void AddPolicy(const std::string policy_name,
                 bool is_cloud,
                 base::Value value) {
    policies_.Set(policy_name, PolicyLevel::POLICY_LEVEL_MANDATORY,
                  PolicyScope::POLICY_SCOPE_MACHINE,
                  is_cloud ? PolicySource::POLICY_SOURCE_CLOUD
                           : PolicySource::POLICY_SOURCE_PLATFORM,
                  std::move(value), nullptr);
    if (policy_name != key::kEnableExperimentalPolicies) {
      handler_list_->AddHandler(
          std::make_unique<StubPolicyHandler>(policy_name));
    }
  }

  void ApplySettings() {
    handler_list_->ApplyPolicySettings(
        policies_, &prefs_, &errors_, &deprecated_policies_, &future_policies_);
  }

  void CreateHandlerList(bool are_future_policies_allowed_by_default = false) {
    handler_list_ = std::make_unique<ConfigurationPolicyHandlerList>(
        ConfigurationPolicyHandlerList::
            PopulatePolicyHandlerParametersCallback(),
        base::BindRepeating(
            &ConfigurationPolicyHandlerListTest::GetPolicyDetails,
            base::Unretained(this)),
        are_future_policies_allowed_by_default);
  }

  PrefValueMap* prefs() { return &prefs_; }

  const PolicyDetails* GetPolicyDetails(const std::string& policy_name) {
    return &details_;
  }
  PolicyDetails* details() { return &details_; }

  void VerifyPolicyAndPref(const std::string& policy_name,
                           bool in_pref,
                           bool in_deprecated = false,
                           bool in_future = false) {
    int pref_value;

    ASSERT_EQ(in_pref, prefs_.GetInteger(policy_name, &pref_value));
    if (in_pref) {
      EXPECT_EQ(kPolicyValue, pref_value);
    }

    // Pref filter never affects PolicyMap.
    const base::Value* policy_value =
        policies_.GetValue(policy_name, base::Value::Type::INTEGER);
    ASSERT_TRUE(policy_value);
    EXPECT_EQ(kPolicyValue, policy_value->GetInt());

    EXPECT_EQ(in_deprecated, deprecated_policies_.find(policy_name) !=
                                 deprecated_policies_.end());
    EXPECT_EQ(in_future,
              future_policies_.find(policy_name) != future_policies_.end());
  }

  void ClearErrors() { errors_.Clear(); }

  void ClearPolicies() { policies_.Clear(); }

  void ClearPrefs() { prefs_.Clear(); }

  bool IsErrorsEmpty() const { return errors_.empty(); }

  std::u16string GetErrorMessages(const std::string& policy_name) {
    return errors_.GetErrorMessages(policy_name);
  }

  void SetPolicy(const std::string& policy,
                 PolicyLevel level,
                 PolicyScope scope,
                 PolicySource source,
                 base::Value value) {
    policies_.Set(policy, level, scope, source, std::move(value), nullptr);
  }

  void SetPolicyEntry(const std::string& policy, PolicyMap::Entry entry) {
    policies_.Set(policy, std::move(entry));
  }

  void RegisterHandler(const std::string& policy_name) {
    handler_list_->AddHandler(std::make_unique<StubPolicyHandler>(policy_name));
  }

 private:
  PrefValueMap prefs_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PoliciesSet deprecated_policies_;
  PoliciesSet future_policies_;
  PolicyDetails details_{false, false, kProfile, kSourceRestrictionNone, 0, {}};

  std::unique_ptr<ConfigurationPolicyHandlerList> handler_list_;
};

TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithNormalPolicy) {
  AddSimplePolicy();
  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
}

// Future policy will be filter out unless it's whitelisted by
// kEnableExperimentalPolicies.
TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithFuturePolicy) {
  AddSimplePolicy();
  details()->is_future = true;

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false,
                      /*in_deprecated=*/false, /*in_future=*/true);

  // Whitelist a different policy.
  base::ListValue enabled_future_policies;
  enabled_future_policies.Append(kPolicyName2);
  AddPolicy(key::kEnableExperimentalPolicies, /*is_cloud=*/true,
            base::Value(enabled_future_policies.Clone()));

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false,
                      /*in_deprecated=*/false, /*in_future=*/true);

  // Whitelist the policy.
  enabled_future_policies.Append(base::Value(kPolicyName));
  AddPolicy(key::kEnableExperimentalPolicies, /*is_cloud=*/true,
            base::Value(std::move(enabled_future_policies)));

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
}

// Future policy will be filter out unless it's whitelisted by
// kEnableExperimentalPolicies or the feature kFuturePoliciesOnDesktopAndroid is
// enabled.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithFuturePolicyOnDesktopAndroid) {
  AddSimplePolicy();
  details()->is_future = true;

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false,
                      /*in_deprecated=*/false, /*in_future=*/true);

  // Future policy will not be filtered out if kFuturePoliciesOnDesktopAndroid
  // is enabled (for Desktop Android dogfooders, see
  // https://crbug.com/452666657).
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFuturePoliciesOnDesktopAndroid);

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
}
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithoutFutureFilterPolicy) {
  CreateHandlerList(true);
  AddSimplePolicy();
  details()->is_future = true;

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
}

// Device platform policy will be filtered out.
TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithPlatformDevicePolicy) {
  AddSimplePolicy();
  details()->scope = kDevice;

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);

  AddPolicy(kPolicyName2, /*is_cloud=*/false, base::Value(kPolicyValue));

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName2, /*in_pref=*/false);
}

// Deprecated policy won't be filtered out.
TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithDeprecatedPolicy) {
  AddSimplePolicy();
  details()->is_deprecated = true;

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true, /*in_deprecated=*/true);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ConfigurationPolicyHandlerListTest, ApplySettingsWithCloudOnlyPolicy) {
  details()->source_restriction = kSourceRestrictionCloudOnly;

  // Cloud source: allowed.
  AddPolicy(kPolicyName, /*is_cloud=*/true, base::Value(kPolicyValue));
  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
  EXPECT_TRUE(IsErrorsEmpty());

  // Platform source: disallowed.
  ClearPrefs();
  AddPolicy(kPolicyName, /*is_cloud=*/false, base::Value(kPolicyValue));
  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false);
  EXPECT_EQ(GetErrorMessages(kPolicyName),
            l10n_util::GetStringUTF16(IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR));
  ClearErrors();

  // Cloud source from Ash: allowed.
  ClearPrefs();
  SetPolicy(kPolicyName, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
            POLICY_SOURCE_CLOUD_FROM_ASH, base::Value(kPolicyValue));
  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
  EXPECT_TRUE(IsErrorsEmpty());

  // Command line source: allowed on Android, disallowed otherwise.
  ClearPrefs();
  SetPolicy(kPolicyName, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
            POLICY_SOURCE_COMMAND_LINE, base::Value(kPolicyValue));
  ApplySettings();
#if BUILDFLAG(IS_ANDROID)
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
  EXPECT_TRUE(IsErrorsEmpty());
#else
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false);
  EXPECT_EQ(GetErrorMessages(kPolicyName),
            l10n_util::GetStringUTF16(IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR));
  ClearErrors();
#endif
}

// TODO(crbug.com/491119520): Remove this test once the CloudReportingEnabled
// exemption is removed.
TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithCloudOnlyPolicyCloudReportingEnabled) {
  details()->source_restriction = kSourceRestrictionCloudOnly;

  // Cloud source: allowed.
  AddPolicy(key::kCloudReportingEnabled, /*is_cloud=*/true,
            base::Value(kPolicyValue));
  ApplySettings();
  VerifyPolicyAndPref(key::kCloudReportingEnabled, /*in_pref=*/true);
  EXPECT_TRUE(IsErrorsEmpty());

  // Platform source: allowed (for now, until we figure out if it's safe to
  // remove the exception for this policy).
  ClearPrefs();
  AddPolicy(key::kCloudReportingEnabled, /*is_cloud=*/false,
            base::Value(kPolicyValue));
  ApplySettings();
  VerifyPolicyAndPref(key::kCloudReportingEnabled, /*in_pref=*/true);
  EXPECT_TRUE(IsErrorsEmpty());
}

TEST_F(ConfigurationPolicyHandlerListTest,
       ApplySettingsWithCloudOnlyPolicyMerged) {
  details()->source_restriction = kSourceRestrictionCloudOnly;

  RegisterHandler(kPolicyName);

  // Merged source with all cloud conflicts: allowed.
  {
    PolicyMap::Entry conflict1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                               POLICY_SOURCE_CLOUD, base::Value(kPolicyValue),
                               nullptr);
    PolicyMap::Entry conflict2(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                               POLICY_SOURCE_CLOUD, base::Value(kPolicyValue),
                               nullptr);
    PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                           POLICY_SOURCE_MERGED, base::Value(kPolicyValue),
                           nullptr);
    entry.conflicts.emplace_back(PolicyMap::ConflictType::None,
                                 std::move(conflict1));
    entry.conflicts.emplace_back(PolicyMap::ConflictType::None,
                                 std::move(conflict2));
    SetPolicyEntry(kPolicyName, std::move(entry));
  }

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
  EXPECT_TRUE(IsErrorsEmpty());

  // Merged source with mixed conflicts: disallowed.
  ClearPrefs();
  {
    PolicyMap::Entry conflict1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                               POLICY_SOURCE_CLOUD, base::Value(kPolicyValue),
                               nullptr);
    PolicyMap::Entry conflict2(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                               POLICY_SOURCE_PLATFORM,
                               base::Value(kPolicyValue), nullptr);
    PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                           POLICY_SOURCE_MERGED, base::Value(kPolicyValue),
                           nullptr);
    entry.conflicts.emplace_back(PolicyMap::ConflictType::None,
                                 std::move(conflict1));
    entry.conflicts.emplace_back(PolicyMap::ConflictType::None,
                                 std::move(conflict2));
    SetPolicyEntry(kPolicyName, std::move(entry));
  }

  ApplySettings();
  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false);
  EXPECT_EQ(GetErrorMessages(kPolicyName),
            l10n_util::GetStringUTF16(IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR));
  ClearErrors();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_DESKTOP_ANDROID)
TEST_F(ConfigurationPolicyHandlerListTest, DesktopAndroidBlocklist_Default) {
  base::test::ScopedFeatureList feature_list;

  AddSimplePolicy();
  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
}

TEST_F(ConfigurationPolicyHandlerListTest, DesktopAndroidBlocklist_Blocked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDesktopAndroidPolicy,
      {{features::kDesktopAndroidPolicyBlocklist.name, kPolicyName}});

  AddSimplePolicy();
  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false);
}

TEST_F(ConfigurationPolicyHandlerListTest,
       DesktopAndroidBlocklist_AllowedAndBlocked) {
  const char kPolicyName3[] = "PolicyName3";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDesktopAndroidPolicy,
      {{features::kDesktopAndroidPolicyBlocklist.name,
        base::StrCat({kPolicyName2, ", ", kPolicyName3})}});

  AddSimplePolicy();
  AddPolicy(kPolicyName2, /*is_cloud=*/true, base::Value(kPolicyValue));

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/true);
  VerifyPolicyAndPref(kPolicyName2, /*in_pref=*/false);
}

// Test that other filters still works.
TEST_F(ConfigurationPolicyHandlerListTest,
       DesktopAndroidBlocklist_AllowedButBlockedByOtherFilter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDesktopAndroidPolicy,
      {{features::kDesktopAndroidPolicyBlocklist.name, kPolicyName2}});

  AddSimplePolicy();
  details()->is_future = true;

  ApplySettings();

  VerifyPolicyAndPref(kPolicyName, /*in_pref=*/false, /*in_deprecated=*/false,
                      /*in_future=*/true);
}
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

}  // namespace policy
