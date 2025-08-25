// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/ntp_shortcuts_policy_handler.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_store.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/field_validation_test_utils.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ntp_tiles::EnterpriseShortcut;
using ntp_tiles::EnterpriseShortcutsStore;
using testing::AllOf;
using testing::ElementsAre;

namespace policy {

namespace {

// Represents field values for NTPShortcuts policy, used for generating
// policy value entries.
struct TestShortcut {
  std::optional<std::string> name;
  std::optional<std::string> url;
  std::optional<bool> allow_user_edit;
  std::optional<bool> allow_user_delete;
};

// Used for tests that require a list of valid shortcuts.
TestShortcut kValidTestShortcuts[] = {
    {.name = "work name",
     .url = "https://work.com/",
     .allow_user_edit = true,
     .allow_user_delete = true},
    {.name = "docs name",
     .url = "https://docs.com/",
     .allow_user_edit = false,
     .allow_user_delete = false},
    {.name = "mail name", .url = "https://mail.com/"},
};

// Used for tests that require shortcuts with missing required fields.
TestShortcut kMissingRequiredFieldsTestShortcuts[] = {
    {.url = "https://missing_name.com/"},
    {.name = "missing_url name"},
};

// Used for tests that require shortcuts with empty required fields.
TestShortcut kEmptyFieldTestShortcuts[] = {
    {.name = "", .url = "https://empty_name.com/"},
    {.name = "empty_url name", .url = ""},
};

// Used for tests that require a shortcut with unknown field.
TestShortcut kUnknownFieldTestShortcuts[] = {
    {.name = "work name", .url = "https://work.com/"},
};

// Used for tests that require a list of shortcuts with a duplicated url,
// but at least one valid entry.
TestShortcut kUrlNotUniqueTestShortcuts[] = {
    {.name = "work name", .url = "https://work.com"},
    {.name = "also work name", .url = "https://work.com/"},
    {.name = "docs name", .url = "https://docs.com/"},
    {.name = "another work name", .url = "https://work.com/"},
};

// Used for tests that require a list of shortcuts with a duplicated url
// and no valid entry.
TestShortcut kNoUniqueUrlTestShortcuts[] = {
    {.name = "work name", .url = "https://work.com/"},
    {.name = "also work name", .url = "https://work.com/"},
    {.name = "another work name", .url = "https://work.com"},
};

// Used for tests that require a shortcut with invalid non-empty URLs.
TestShortcut kInvalidUrlTestShortcuts[] = {
    {.name = "invalid1 name", .url = "work"},
    {.name = "invalid2 name", .url = "www.notvalidurl"},
    {.name = "invalid3 name", .url = "invalid/url"},
};

base::Value::Dict GenerateNTPShortcutPolicyEntry(TestShortcut test_case) {
  base::Value::Dict entry;
  if (test_case.name.has_value()) {
    entry.Set(NTPShortcutsPolicyHandler::kName, test_case.name.value());
  }
  if (test_case.url.has_value()) {
    entry.Set(NTPShortcutsPolicyHandler::kUrl, test_case.url.value());
  }
  if (test_case.allow_user_edit.has_value()) {
    entry.Set(NTPShortcutsPolicyHandler::kAllowUserEdit,
              test_case.allow_user_edit.value());
  }
  if (test_case.allow_user_delete.has_value()) {
    entry.Set(NTPShortcutsPolicyHandler::kAllowUserDelete,
              test_case.allow_user_delete.value());
  }
  return entry;
}

// Returns a matcher that accepts entries for the pref corresponding to the
// NTP shortcuts policy. Field values are obtained from |test_case|.
testing::Matcher<const base::Value&> IsNTPShortcutEntry(
    TestShortcut test_case) {
  return AllOf(
      HasStringField(EnterpriseShortcutsStore::kDictionaryKeyTitle,
                     test_case.name.value()),
      HasStringField(EnterpriseShortcutsStore::kDictionaryKeyUrl,
                     test_case.url.value()),
      HasIntegerField(
          EnterpriseShortcutsStore::kDictionaryKeyPolicyOrigin,
          static_cast<int>(EnterpriseShortcut::PolicyOrigin::kNtpShortcuts)),
      HasBooleanField(EnterpriseShortcutsStore::kDictionaryKeyIsHiddenByUser,
                      false),
      HasBooleanField(EnterpriseShortcutsStore::kDictionaryKeyAllowUserEdit,
                      test_case.allow_user_edit.value_or(false)),
      HasBooleanField(EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete,
                      test_case.allow_user_delete.value_or(false)));
}

MATCHER_P(HasValidationError,
          expected_str,
          base::StringPrintf("%s error message `%s` for `NTPShortcuts`",
                             negation ? "does not contain" : "contains",
                             base::UTF16ToUTF8(expected_str).c_str())) {
  return arg->HasError(key::kNTPShortcuts) &&
         arg->GetErrorMessages(key::kNTPShortcuts).find(expected_str) !=
             std::wstring::npos;
}

MATCHER_P(HasValidationWarning,
          expected_str,
          base::StringPrintf("%s warning message `%s` for `NTPShortcuts`",
                             negation ? "does not contain" : "contains",
                             base::UTF16ToUTF8(expected_str).c_str())) {
  return arg->HasError(key::kNTPShortcuts) &&
         arg->GetErrorMessages(key::kNTPShortcuts,
                               PolicyMap::MessageType::kWarning)
                 .find(expected_str) != std::wstring::npos;
}

}  // namespace

class NTPShortcutsPolicyHandlerTest : public testing::Test {
 public:
  NTPShortcutsPolicyHandlerTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(ntp_tiles::kNtpEnterpriseShortcuts);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  NTPShortcutsPolicyHandler handler_{
      Schema::Wrap(policy::GetChromeSchemaData())};
  policy::PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
};

TEST_F(NTPShortcutsPolicyHandlerTest, PolicyNotSet) {
  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                               nullptr));
}

TEST_F(NTPShortcutsPolicyHandlerTest, ValidNTPShortcuts_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(ntp_tiles::kNtpEnterpriseShortcuts);

  base::Value::List policy_value;
  for (const auto& test_case : kValidTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                               nullptr));
}

TEST_F(NTPShortcutsPolicyHandlerTest, ValidNTPShortcuts) {
  base::Value::List policy_value;
  for (const auto& test_case : kValidTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  handler_.ApplyPolicySettings(policies_, &prefs_);
  base::Value* shortcuts = nullptr;
  ASSERT_TRUE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                              &shortcuts));
  ASSERT_NE(shortcuts, nullptr);
  ASSERT_TRUE(shortcuts->is_list());
  EXPECT_THAT(shortcuts->GetList(),
              ElementsAre(IsNTPShortcutEntry(kValidTestShortcuts[0]),
                          IsNTPShortcutEntry(kValidTestShortcuts[1]),
                          IsNTPShortcutEntry(kValidTestShortcuts[2])));
}

TEST_F(NTPShortcutsPolicyHandlerTest, InvalidFormat) {
  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(false), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.HasError(key::kNTPShortcuts));
}

TEST_F(NTPShortcutsPolicyHandlerTest, TooManyNTPShortcuts) {
  // Policy value has one list entry over the max allowed.
  base::Value::List policy_value;
  for (int i = 0; i <= NTPShortcutsPolicyHandler::kMaxNtpShortcuts; ++i) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(
        {.name = base::StringPrintf("name %d", i),
         .url = base::StringPrintf("https://site_%d.com/", i)}));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationError(l10n_util::GetStringFUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_MAX_SHORTCUTS_LIMIT_ERROR,
                            base::NumberToString16(
                                NTPShortcutsPolicyHandler::kMaxNtpShortcuts))));
}

TEST_F(NTPShortcutsPolicyHandlerTest, MissingRequiredFieldWithValidShortcuts) {
  base::Value::List policy_value;
  for (const auto& test_case : kMissingRequiredFieldsTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }
  policy_value.Append(GenerateNTPShortcutPolicyEntry(kValidTestShortcuts[0]));

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  // CheckPolicySettings should return true because there are valid shortcuts.
  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));

  // There should be warnings for the invalid shortcuts.
  EXPECT_THAT(&errors_,
              HasValidationWarning(
                  u"Error at NTPShortcuts[1]: Schema validation error: Missing "
                  u"or invalid required property: url"));

  handler_.ApplyPolicySettings(policies_, &prefs_);
  base::Value* shortcuts = nullptr;
  ASSERT_TRUE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                              &shortcuts));
  ASSERT_NE(shortcuts, nullptr);
  ASSERT_TRUE(shortcuts->is_list());

  // Only the valid shortcuts should be in the prefs.
  EXPECT_THAT(shortcuts->GetList(),
              ElementsAre(IsNTPShortcutEntry(kValidTestShortcuts[0])));
}

TEST_F(NTPShortcutsPolicyHandlerTest, MissingRequiredField) {
  base::Value::List policy_value;
  for (const auto& test_case : kMissingRequiredFieldsTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.HasError(key::kNTPShortcuts));
}

TEST_F(NTPShortcutsPolicyHandlerTest, UrlNotUnique) {
  base::Value::List policy_value;
  for (const auto& test_case : kUrlNotUniqueTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationWarning(l10n_util::GetStringFUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_DUPLICATED_URL,
                            u"https://work.com/")));

  handler_.ApplyPolicySettings(policies_, &prefs_);
  base::Value* shortcuts = nullptr;
  ASSERT_TRUE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                              &shortcuts));
  ASSERT_NE(shortcuts, nullptr);
  ASSERT_TRUE(shortcuts->is_list());
  EXPECT_THAT(shortcuts->GetList(),
              ElementsAre(IsNTPShortcutEntry(kUrlNotUniqueTestShortcuts[2])));
}

TEST_F(NTPShortcutsPolicyHandlerTest, NoUniqueUrl) {
  base::Value::List policy_value;
  for (const auto& test_case : kNoUniqueUrlTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationWarning(l10n_util::GetStringFUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_DUPLICATED_URL,
                            u"https://work.com/")));
  EXPECT_THAT(&errors_, HasValidationError(l10n_util::GetStringUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_NO_VALID_PROVIDER)));
}

TEST_F(NTPShortcutsPolicyHandlerTest, EmptyRequiredField) {
  base::Value::List policy_value;
  for (const auto& test_case : kEmptyFieldTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationWarning(l10n_util::GetStringUTF16(
                            IDS_SEARCH_POLICY_SETTINGS_NAME_IS_EMPTY)));
  EXPECT_THAT(&errors_, HasValidationWarning(l10n_util::GetStringUTF16(
                            IDS_SEARCH_POLICY_SETTINGS_URL_IS_EMPTY)));
}

TEST_F(NTPShortcutsPolicyHandlerTest, UnknownField) {
  constexpr char kUnknownFieldName[] = "unknown_field";

  base::Value::Dict entry =
      GenerateNTPShortcutPolicyEntry(kUnknownFieldTestShortcuts[0]);
  entry.Set(kUnknownFieldName, true);
  base::Value::List policy_value;
  policy_value.Append(std::move(entry));

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  // A warning is registered during policy validation, but valid fields are
  // still used for building a new shortcut.
  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());

  handler_.ApplyPolicySettings(policies_, &prefs_);
  base::Value* shortcuts = nullptr;
  ASSERT_TRUE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                              &shortcuts));
  ASSERT_NE(shortcuts, nullptr);
  ASSERT_TRUE(shortcuts->is_list());
  EXPECT_THAT(shortcuts->GetList(),
              ElementsAre(IsNTPShortcutEntry(kUnknownFieldTestShortcuts[0])));
}

TEST_F(NTPShortcutsPolicyHandlerTest, InvalidUrlError) {
  base::Value::List policy_value;
  for (const auto& test_case : kInvalidUrlTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationWarning(l10n_util::GetStringUTF16(
                            IDS_POLICY_INVALID_URL_ERROR)));
  EXPECT_THAT(&errors_, HasValidationError(l10n_util::GetStringUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_NO_VALID_PROVIDER)));
}

TEST_F(NTPShortcutsPolicyHandlerTest, InvalidUrlWarning) {
  base::Value::List policy_value;
  for (const auto& test_case : kInvalidUrlTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }
  policy_value.Append(GenerateNTPShortcutPolicyEntry(kValidTestShortcuts[0]));

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationWarning(l10n_util::GetStringUTF16(
                            IDS_POLICY_INVALID_URL_ERROR)));

  handler_.ApplyPolicySettings(policies_, &prefs_);
  base::Value* shortcuts = nullptr;
  ASSERT_TRUE(prefs_.GetValue(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
                              &shortcuts));
  ASSERT_NE(shortcuts, nullptr);
  ASSERT_TRUE(shortcuts->is_list());
  EXPECT_THAT(shortcuts->GetList(),
              ElementsAre(IsNTPShortcutEntry(kValidTestShortcuts[0])));
}

TEST_F(NTPShortcutsPolicyHandlerTest, NoValidEntry) {
  base::Value::List policy_value;
  for (const auto& test_case : kInvalidUrlTestShortcuts) {
    policy_value.Append(GenerateNTPShortcutPolicyEntry(test_case));
  }
  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationError(l10n_util::GetStringUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_NO_VALID_PROVIDER)));
}

TEST_F(NTPShortcutsPolicyHandlerTest, EmptyList) {
  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(base::Value::List()), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_, HasValidationError(l10n_util::GetStringUTF16(
                            IDS_POLICY_NTP_SHORTCUTS_NO_VALID_PROVIDER)));
}

TEST_F(NTPShortcutsPolicyHandlerTest, NameTooLong) {
  base::Value::List policy_value;
  policy_value.Append(GenerateNTPShortcutPolicyEntry(
      {.name = std::string(
           NTPShortcutsPolicyHandler::kMaxNtpShortcutTextLength + 1, 'a'),
       .url = "https://work.com/"}));

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_,
              HasValidationWarning(l10n_util::GetStringFUTF16(
                  IDS_POLICY_NTP_SHORTCUTS_NAME_TOO_LONG,
                  base::NumberToString16(
                      NTPShortcutsPolicyHandler::kMaxNtpShortcutTextLength))));
}

TEST_F(NTPShortcutsPolicyHandlerTest, UrlTooLong) {
  base::Value::List policy_value;
  policy_value.Append(GenerateNTPShortcutPolicyEntry(
      {.name = "work",
       .url = base::StringPrintf(
           "https://%s.com/",
           std::string(NTPShortcutsPolicyHandler::kMaxNtpShortcutTextLength,
                       'a')
               .c_str())}));

  policies_.Set(key::kNTPShortcuts, policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                base::Value(std::move(policy_value)), nullptr);

  ASSERT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_THAT(&errors_,
              HasValidationWarning(l10n_util::GetStringFUTF16(
                  IDS_POLICY_NTP_SHORTCUTS_URL_TOO_LONG,
                  base::NumberToString16(
                      NTPShortcutsPolicyHandler::kMaxNtpShortcutTextLength))));
}

}  // namespace policy
