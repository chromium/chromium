// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/idle_timeout_policy_handler.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/enterprise/idle/action_type.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_idle {

using base::UTF8ToUTF16;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

class IdleTimeoutPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    // Some action types require SyncDisabled=true, so set it for most tests.
    SetPolicyValue(policy::key::kSyncDisabled, base::Value(true));
  }

  void SetPolicyValue(
      const std::string& policy,
      base::Value value,
      policy::PolicyScope scope = policy::POLICY_SCOPE_MACHINE) {
    policies_.Set(policy, policy::POLICY_LEVEL_MANDATORY, scope,
                  policy::POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
  }

  bool CheckPolicySettings() {
    bool results[] = {
        timeout_handler_.CheckPolicySettings(policies_, &errors_),
        actions_handler_.CheckPolicySettings(policies_, &errors_),
    };
    return base::ranges::all_of(base::span(results), std::identity{});
  }

  void ApplyPolicySettings() {
    timeout_handler_.ApplyPolicySettings(policies_, &prefs_);
    actions_handler_.ApplyPolicySettings(policies_, &prefs_);
    actions_handler_.PrepareForDisplaying(&policies_);
  }

  void CheckAndApplyPolicySettings() {
    if (CheckPolicySettings()) {
      ApplyPolicySettings();
    }
  }

  PrefValueMap& prefs() { return prefs_; }
  policy::PolicyMap& policies() { return policies_; }

  std::vector<std::u16string> errors() {
    std::vector<std::u16string> strings;
    base::ranges::transform(errors_, std::back_inserter(strings),
                            [](const auto& it) { return it.second.message; });
    return strings;
  }

 private:
  policy::PolicyMap policies_;
  policy::PolicyErrorMap errors_;
  PrefValueMap prefs_;
  policy::Schema schema_ = policy::Schema::Wrap(policy::GetChromeSchemaData());
  IdleTimeoutPolicyHandler timeout_handler_;
  IdleTimeoutActionsPolicyHandler actions_handler_ =
      IdleTimeoutActionsPolicyHandler(schema_);
};

TEST_F(IdleTimeoutPolicyHandlerTest, PoliciesNotSet) {
  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_THAT(errors(), IsEmpty());

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, JustTimeout) {
  // IdleTimeout is set, but not IdleTimeoutActions.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                                 UTF8ToUTF16(policy::key::kIdleTimeoutActions));
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, JustActions) {
  // IdleTimeoutActions is set, but not IdleTimeout.
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(base::Value::List()));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                                 UTF8ToUTF16(policy::key::kIdleTimeout));
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, InvalidTimeoutPolicyType) {
  // Give an integer to a string policy.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value("invalid"));
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(base::Value::List()));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      UTF8ToUTF16(base::Value::GetTypeName(base::Value::Type::INTEGER)));
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, InvalidActionsPolicyType) {
  // Give a string to a string-enum policy.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(5));
  SetPolicyValue(policy::key::kIdleTimeoutActions, base::Value("invalid"));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_SCHEMA_VALIDATION_ERROR,
      u"Policy type mismatch: expected: \"list\", actual: \"string\".");
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, InvalidActionWrongType) {
  // IdleTimeoutActions is a list, but one of the elements is not even a string.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(5));
  base::Value::List list;
  list.Append("clear_browsing_history");
  list.Append(34);
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_ERROR_WITH_PATH,
      UTF8ToUTF16(policy::key::kIdleTimeoutActions) + u"[1]",
      l10n_util::GetStringFUTF16(
          IDS_POLICY_SCHEMA_VALIDATION_ERROR,
          u"Policy type mismatch: expected: \"string\", actual: \"integer\"."));
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  EXPECT_THAT(pref_value->GetList(), testing::ElementsAre(static_cast<int>(
                                         ActionType::kClearBrowsingHistory)));
}

TEST_F(IdleTimeoutPolicyHandlerTest, ValidConfiguration) {
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));
  base::Value::List list;
  list.Append("clear_browsing_history");
  list.Append("clear_cookies_and_other_site_data");
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  EXPECT_THAT(errors(), IsEmpty());

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(15)), *pref_value);

  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(pref_value->GetList(),
              testing::ElementsAre(
                  static_cast<int>(ActionType::kClearBrowsingHistory),
                  static_cast<int>(ActionType::kClearCookiesAndOtherSiteData)));
}

TEST_F(IdleTimeoutPolicyHandlerTest, OneMinuteMinimum) {
  // Set the policy to 0, which should clamp the pref to 1.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(0));
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(base::Value::List()));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringFUTF16(IDS_POLICY_OUT_OF_RANGE_ERROR, u"0");
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(1)), *pref_value);
}

TEST_F(IdleTimeoutPolicyHandlerTest, ActionNotRecognized) {
  // IdleTimeoutActions is a list, but one of the elements is not recognized
  // as a valid option. Recognized actions are applied, but not the others.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(5));
  base::Value::List list;
  list.Append("clear_browsing_history");
  list.Append("clear_cookies_and_other_site_data");
  list.Append("added_in_future_version_of_chrome");
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_ERROR_WITH_PATH,
      UTF8ToUTF16(policy::key::kIdleTimeoutActions) + u"[2]",
      l10n_util::GetStringFUTF16(IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                                 u"Invalid value for string"));
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error));

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(pref_value->GetList(),
              testing::ElementsAre(
                  static_cast<int>(ActionType::kClearBrowsingHistory),
                  static_cast<int>(ActionType::kClearCookiesAndOtherSiteData)));
}

TEST_F(IdleTimeoutPolicyHandlerTest, AllActions) {
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));
  base::Value::List list;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  list.Append("close_browsers");
  list.Append("show_profile_picker");
  list.Append("clear_download_history");
  list.Append("clear_hosted_app_data");
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_IOS)
  list.Append("clear_site_settings");
  list.Append("reload_pages");
#endif
  list.Append("clear_browsing_history");
  list.Append("clear_cookies_and_other_site_data");
  list.Append("clear_cached_images_and_files");
  list.Append("clear_password_signin");
  list.Append("clear_autofill");

  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  // Should have no errors.
  EXPECT_THAT(errors(), IsEmpty());

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(15)), *pref_value);

  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(pref_value->GetList(),
              testing::ElementsAre(
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
                  static_cast<int>(ActionType::kCloseBrowsers),
                  static_cast<int>(ActionType::kShowProfilePicker),
                  static_cast<int>(ActionType::kClearDownloadHistory),
                  static_cast<int>(ActionType::kClearHostedAppData),
#endif  // !BUILDFLAG(IS_ANDROID) !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_IOS)
                  static_cast<int>(ActionType::kClearSiteSettings),
                  static_cast<int>(ActionType::kReloadPages),
#endif  // !BUILDFLAG(IS_IOS)
                  static_cast<int>(ActionType::kClearBrowsingHistory),
                  static_cast<int>(ActionType::kClearCookiesAndOtherSiteData),
                  static_cast<int>(ActionType::kClearCachedImagesAndFiles),
                  static_cast<int>(ActionType::kClearPasswordSignin),
                  static_cast<int>(ActionType::kClearAutofill)));
}

// When browser sign in is disabled by policy, the clear actions should
// be applied and the error map and messages should be empty.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(IdleTimeoutPolicyHandlerTest, BrowserSigninDisabled) {
  SetPolicyValue(policy::key::kSyncDisabled, base::Value(false));
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));
  SetPolicyValue(policy::key::kBrowserSignin, base::Value(0));

  base::Value::List list;
  list.Append("clear_browsing_history");
  list.Append("clear_cookies_and_other_site_data");
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  EXPECT_THAT(errors(), IsEmpty());

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(15)), *pref_value);

  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(pref_value->GetList(),
              testing::ElementsAre(
                  static_cast<int>(ActionType::kClearBrowsingHistory),
                  static_cast<int>(ActionType::kClearCookiesAndOtherSiteData)));
}
#endif

TEST_F(IdleTimeoutPolicyHandlerTest, SyncTypesDisabledForClearActions) {
  // Start with sync prefs enabled so we can sense that they have changed.
  prefs().SetBoolean(syncer::prefs::internal::kSyncAutofill, true);
  prefs().SetBoolean(syncer::prefs::internal::kSyncPreferences, true);
  prefs().SetBoolean(syncer::prefs::internal::kSyncHistory, true);
  prefs().SetBoolean(syncer::prefs::internal::kSyncTabs, true);
  prefs().SetBoolean(syncer::prefs::internal::kSyncSavedTabGroups, true);
  prefs().SetBoolean(syncer::prefs::internal::kSyncPasswords, true);

  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));
  SetPolicyValue(policy::key::kSyncDisabled, base::Value(false));

  base::Value::List list;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  list.Append("close_browsers");
  list.Append("show_profile_picker");
  list.Append("clear_download_history");
  list.Append("clear_hosted_app_data");
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_IOS)
  list.Append("clear_site_settings");
  list.Append("reload_pages");
#endif  // !BUILDFLAG(IS_IOS)
  list.Append("clear_browsing_history");
  list.Append("clear_cookies_and_other_site_data");
  list.Append("clear_cached_images_and_files");
  list.Append("clear_password_signin");
  list.Append("clear_autofill");

  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  EXPECT_THAT(errors(), IsEmpty());
  EXPECT_TRUE(policies()
                  .Get(policy::key::kIdleTimeoutActions)
                  ->HasMessage(policy::PolicyMap::MessageType::kInfo));

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(15)), *pref_value);

  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(pref_value->GetList(),
              testing::ElementsAre(
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
                  static_cast<int>(ActionType::kCloseBrowsers),
                  static_cast<int>(ActionType::kShowProfilePicker),
                  static_cast<int>(ActionType::kClearDownloadHistory),
                  static_cast<int>(ActionType::kClearHostedAppData),
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_IOS)
                  static_cast<int>(ActionType::kClearSiteSettings),
                  static_cast<int>(ActionType::kReloadPages),
#endif  // !BUILDFLAG(IS_IOS)
                  static_cast<int>(ActionType::kClearBrowsingHistory),
                  static_cast<int>(ActionType::kClearCookiesAndOtherSiteData),
                  static_cast<int>(ActionType::kClearCachedImagesAndFiles),
                  static_cast<int>(ActionType::kClearPasswordSignin),
                  static_cast<int>(ActionType::kClearAutofill)));

  bool enabled;
  ASSERT_TRUE(
      prefs().GetBoolean(syncer::prefs::internal::kSyncPreferences, &enabled));
#if BUILDFLAG(IS_IOS)
  EXPECT_TRUE(enabled);
#else
  EXPECT_FALSE(enabled);
#endif  // BUILDFLAG(IS_IOS)
  ASSERT_TRUE(
      prefs().GetBoolean(syncer::prefs::internal::kSyncHistory, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs().GetBoolean(syncer::prefs::internal::kSyncTabs, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs().GetBoolean(syncer::prefs::internal::kSyncSavedTabGroups,
                                 &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(
      prefs().GetBoolean(syncer::prefs::internal::kSyncAutofill, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(
      prefs().GetBoolean(syncer::prefs::internal::kSyncPasswords, &enabled));
  EXPECT_FALSE(enabled);
}

TEST_F(IdleTimeoutPolicyHandlerTest,
       IdleTimeoutPolicyAppliesCorrectlyAsUserPolicy) {
  // Initialize the pref to false to detect the pref changes when the policy is
  // set.
  prefs().SetBoolean(syncer::prefs::internal::kSyncAutofill, true);

  base::Value::List list;
  list.Append("clear_browsing_history");
  list.Append("clear_cookies_and_other_site_data");

  // Set the policy scope to user and check that
  // the policy is not set on iOS and set on other platforms.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15),
                 policy::POLICY_SCOPE_USER);
  SetPolicyValue(policy::key::kIdleTimeoutActions, base::Value(std::move(list)),
                 policy::POLICY_SCOPE_USER);
  CheckAndApplyPolicySettings();

#if BUILDFLAG(IS_IOS)
  // Should have an error.
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_NOT_SUPPORTED_AS_USER_POLICY_ON_IOS);
  EXPECT_THAT(errors(), UnorderedElementsAre(expected_error, expected_error));

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
#else
  // Should not have an error.
  EXPECT_THAT(errors(), IsEmpty());

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(15)), *pref_value);

  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(pref_value->GetList(),
              testing::ElementsAre(
                  static_cast<int>(ActionType::kClearBrowsingHistory),
                  static_cast<int>(ActionType::kClearCookiesAndOtherSiteData)));
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace enterprise_idle
