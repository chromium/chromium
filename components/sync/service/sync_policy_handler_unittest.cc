// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_policy_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(SyncPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(prefs.GetValue(prefs::internal::kSyncManaged, nullptr));
}

TEST(SyncPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(false), nullptr);
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Sync should not set the pref.
  EXPECT_FALSE(prefs.GetValue(prefs::internal::kSyncManaged, nullptr));
}

TEST(SyncPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(true), nullptr);
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);

  // Sync should be flagged as managed.
  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::internal::kSyncManaged, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetBool());
}

TEST(SyncPolicyHandlerTest, SyncTypesListDisabled) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::internal::kSyncBookmarks, true);
  prefs.SetBoolean(prefs::internal::kSyncReadingList, true);
  prefs.SetBoolean(prefs::internal::kSyncPreferences, true);
  prefs.SetBoolean(prefs::internal::kSyncAutofill, true);
  prefs.SetBoolean(prefs::internal::kSyncPayments, true);
  prefs.SetBoolean(prefs::internal::kSyncThemes, true);

  // Create a policy that disables some types.
  policy::PolicyMap policy;
  auto disabled_types = base::Value::List()
                            .Append("bookmarks")
                            .Append("readingList")
                            .Append("preferences");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(disabled_types)), nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Prefs in the policy should be disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncBookmarks, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncReadingList, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncPreferences, &enabled));
  EXPECT_FALSE(enabled);

  // Prefs that are not part of the policy are still enabled.
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncAutofill, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncPayments, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncThemes, &enabled));
  EXPECT_TRUE(enabled);
}

// Same as SyncTypesListDisabled but tests autofill specifically.
TEST(SyncPolicyHandlerTest, SyncTypesListDisabledAutofill) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::internal::kSyncBookmarks, true);
  prefs.SetBoolean(prefs::internal::kSyncReadingList, true);
  prefs.SetBoolean(prefs::internal::kSyncPreferences, true);
  prefs.SetBoolean(prefs::internal::kSyncAutofill, true);
  prefs.SetBoolean(prefs::internal::kSyncPayments, true);
  prefs.SetBoolean(prefs::internal::kSyncThemes, true);

  // Create a policy that disables autofill.
  policy::PolicyMap policy;
  auto disabled_types = base::Value::List().Append("autofill");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(disabled_types)), nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Prefs in the policy should be disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncAutofill, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncPayments, &enabled));
  EXPECT_FALSE(enabled);

  // Prefs that are not part of the policy are still enabled.
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncBookmarks, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncReadingList, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncPreferences, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncThemes, &enabled));
  EXPECT_TRUE(enabled);
}

TEST(SyncPolicyHandlerTest, SyncTypesListDisabledInvalidEntry) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::internal::kSyncBookmarks, true);
  prefs.SetBoolean(prefs::internal::kSyncReadingList, true);
  prefs.SetBoolean(prefs::internal::kSyncPreferences, true);
  prefs.SetBoolean(prefs::internal::kSyncAutofill, true);

  // Create a policy that disables some types, but also contains a non-string
  // entry in the list.
  policy::PolicyMap policy;
  auto disabled_types =
      base::Value::List().Append("bookmarks").Append(123).Append("preferences");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(disabled_types)), nullptr);

  SyncPolicyHandler handler;

  // The invalid value should have produced a warning message.
  policy::PolicyErrorMap errors;
  handler.CheckPolicySettings(policy, &errors);
  std::vector<policy::PolicyErrorMap::Data> error_data =
      errors.GetErrors(policy::key::kSyncTypesListDisabled);
  ASSERT_EQ(error_data.size(), 1u);
  EXPECT_EQ(error_data[0].level, policy::PolicyMap::MessageType::kWarning);

  // But the valid entries should still have been applied.
  handler.ApplyPolicySettings(policy, &prefs);

  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncBookmarks, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncPreferences, &enabled));
  EXPECT_FALSE(enabled);

  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncAutofill, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncReadingList, &enabled));
  EXPECT_TRUE(enabled);
}

TEST(SyncPolicyHandlerTest, SyncTypesListDisabledUnknownEntry) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::internal::kSyncBookmarks, true);
  prefs.SetBoolean(prefs::internal::kSyncReadingList, true);
  prefs.SetBoolean(prefs::internal::kSyncPreferences, true);
  prefs.SetBoolean(prefs::internal::kSyncAutofill, true);

  // Create a policy that disables some types, but also contains an unrecognized
  // value (in practice, this could be a previous data type that was deprecated,
  // or a new data type that this version of the browser doesn't know about yet,
  // or just a typo in the configuration).
  policy::PolicyMap policy;
  auto disabled_types = base::Value::List()
                            .Append("bookmarks")
                            .Append("nonexistent")
                            .Append("preferences");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(disabled_types)), nullptr);

  SyncPolicyHandler handler;

  // The invalid value should have produced a warning message.
  policy::PolicyErrorMap errors;
  handler.CheckPolicySettings(policy, &errors);
  std::vector<policy::PolicyErrorMap::Data> error_data =
      errors.GetErrors(policy::key::kSyncTypesListDisabled);
  ASSERT_EQ(error_data.size(), 1u);
  EXPECT_NE(error_data[0].message.find(u"nonexistent"), std::string::npos);
  EXPECT_EQ(error_data[0].level, policy::PolicyMap::MessageType::kWarning);

  // But the valid entries should still have been applied.
  handler.ApplyPolicySettings(policy, &prefs);

  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncBookmarks, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncPreferences, &enabled));
  EXPECT_FALSE(enabled);

  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncAutofill, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncReadingList, &enabled));
  EXPECT_TRUE(enabled);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

TEST(SyncPolicyHandlerOsTest, SyncTypesListDisabled_OsTypes) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::internal::kSyncOsApps, true);
  prefs.SetBoolean(prefs::internal::kSyncOsPreferences, true);
  prefs.SetBoolean(prefs::internal::kSyncWifiConfigurations, true);

  // Create a policy that disables the types.
  policy::PolicyMap policy;
  auto disabled_types = base::Value::List()
                            .Append("osApps")
                            .Append("osPreferences")
                            .Append("osWifiConfigurations");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(disabled_types)), nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Prefs in the policy are disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncOsApps, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncOsPreferences, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(
      prefs.GetBoolean(prefs::internal::kSyncWifiConfigurations, &enabled));
  EXPECT_FALSE(enabled);
}

TEST(SyncPolicyHandlerOsTest, SyncTypesListDisabled_MigratedTypes) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::internal::kSyncOsApps, true);
  prefs.SetBoolean(prefs::internal::kSyncOsPreferences, true);

  // Create a policy that disables the types, but using the original browser
  // policy names from before the SplitSettingsSync launch.
  policy::PolicyMap policy;
  auto disabled_types = base::Value::List()
                            .Append("apps")
                            .Append("wifiConfigurations")
                            .Append("preferences");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(disabled_types)), nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // The equivalent OS types are disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncOsApps, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(
      prefs.GetBoolean(prefs::internal::kSyncWifiConfigurations, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::internal::kSyncOsPreferences, &enabled));
  EXPECT_FALSE(enabled);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace syncer
