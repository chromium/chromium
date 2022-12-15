// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

using password_manager::CreateLeakType;
using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::metrics_util::LeakDialogType;

namespace password_manager {

namespace {

constexpr int GetLeakChangePasswordMessage() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED;
#else
  return IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED;
#endif
}

// Contains information that should be displayed on the leak dialog for
// specified `leak_type`.
const struct {
  // Specifies the test case.
  CredentialLeakType leak_type;
  // The rest of the fields specify what should be displayed for this test case.
  int accept_button_id;
  int cancel_button_id;
  int leak_message_id;
  int leak_title_id;
  bool should_show_cancel_button;
  bool should_check_passwords;
} kLeakTypesTestCases[] = {
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), IDS_OK,
     IDS_CLOSE, GetLeakChangePasswordMessage(),
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, GetLeakChangePasswordMessage(),
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
     IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED,
#else
     IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED,
#endif
     IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM, true, true},
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, GetLeakChangePasswordMessage(),
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, GetLeakChangePasswordMessage(),
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
     IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED,
#else
     IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED,
#endif
     IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM, true, true},
};

struct BulkCheckParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  bool should_check_passwords;
} kBulkCheckTestCases[] = {
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)), false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)), true},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)), true},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)), true}};
}  // namespace

class CredentialLeakDialogUtilsTest : public testing::Test {
 public:
  CredentialLeakDialogUtilsTest() {
#if BUILDFLAG(IS_IOS)
    feature_list_.InitAndEnableFeature(
        features::kIOSEnablePasswordManagerBrandingUpdate);
#elif BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndEnableFeature(
        features::kUnifiedPasswordManagerAndroid);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].accept_button_id),
        GetAcceptButtonLabel(kLeakTypesTestCases[i].leak_type));
  }
}

TEST_F(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetAcceptButtonLabel) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].accept_button_id),
        CreateDialogTraits(kLeakTypesTestCases[i].leak_type)
            ->GetAcceptButtonLabel());
  }
}

TEST_F(CredentialLeakDialogUtilsTest, GetCancelButtonLabel) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].cancel_button_id),
        GetCancelButtonLabel(kLeakTypesTestCases[i].leak_type));
  }
}

TEST_F(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetCancelButtonLabel) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].cancel_button_id),
        CreateDialogTraits(kLeakTypesTestCases[i].leak_type)
            ->GetCancelButtonLabel());
  }
}

TEST_F(CredentialLeakDialogUtilsTest, GetDescription) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    std::u16string expected_message =
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_message_id);
    EXPECT_EQ(expected_message,
              GetDescription(kLeakTypesTestCases[i].leak_type));
  }
}

TEST_F(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetDescription) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_message_id),
        CreateDialogTraits(kLeakTypesTestCases[i].leak_type)->GetDescription());
  }
}

TEST_F(CredentialLeakDialogUtilsTest, GetTitle) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_title_id),
              GetTitle(kLeakTypesTestCases[i].leak_type));
  }
}

TEST_F(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetTitle) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_title_id),
              CreateDialogTraits(kLeakTypesTestCases[i].leak_type)->GetTitle());
  }
}

TEST_F(CredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_check_passwords,
              ShouldCheckPasswords(kLeakTypesTestCases[i].leak_type));
  }
}

TEST_F(CredentialLeakDialogUtilsTest, LeakDialogTraits_ShouldCheckPasswords) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_check_passwords,
              CreateDialogTraits(kLeakTypesTestCases[i].leak_type)
                  ->ShouldCheckPasswords());
  }
}

TEST_F(CredentialLeakDialogUtilsTest, ShouldShowCancelButton) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_show_cancel_button,
              ShouldShowCancelButton(kLeakTypesTestCases[i].leak_type));
  }
}

TEST_F(CredentialLeakDialogUtilsTest, LeakDialogTraits_ShouldShowCancelButton) {
  for (size_t i = 0; i < std::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_show_cancel_button,
              CreateDialogTraits(kLeakTypesTestCases[i].leak_type)
                  ->ShouldShowCancelButton());
  }
}

class BulkCheckCredentialLeakDialogUtilsTest
    : public testing::TestWithParam<BulkCheckParams> {
 public:
  BulkCheckCredentialLeakDialogUtilsTest() {
#if BUILDFLAG(IS_IOS)
    feature_list_.InitAndEnableFeature(
        features::kIOSEnablePasswordManagerBrandingUpdate);
#elif BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndEnableFeature(
        features::kUnifiedPasswordManagerAndroid);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(BulkCheckCredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_check_passwords,
            ShouldCheckPasswords(GetParam().leak_type));
}

TEST_P(BulkCheckCredentialLeakDialogUtilsTest, Buttons) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_check_passwords,
            ShouldShowCancelButton(GetParam().leak_type));
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().should_check_passwords
                                          ? IDS_LEAK_CHECK_CREDENTIALS
                                          : IDS_OK),
            GetAcceptButtonLabel(GetParam().leak_type));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
            GetCancelButtonLabel(GetParam().leak_type));
}

TEST_P(BulkCheckCredentialLeakDialogUtilsTest, Title) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().should_check_passwords
                                          ? IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM
                                          : IDS_CREDENTIAL_LEAK_TITLE_CHANGE),
            GetTitle(GetParam().leak_type));
}

INSTANTIATE_TEST_SUITE_P(InstantiationName,
                         BulkCheckCredentialLeakDialogUtilsTest,
                         testing::ValuesIn(kBulkCheckTestCases));

#if BUILDFLAG(IS_ANDROID)
struct PasswordChangeParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  // The rest of the fields specify what should be displayed for this test case.
  int accept_button_id;
  int cancel_button_id;
  bool should_show_cancel_button;
  bool should_show_change_password_button;
} kPasswordChangeTestCases[] = {
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), IDS_OK,
     0, false, false},
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
     0, false, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)), IDS_OK,
     0, false, false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, false, true},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, false},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, true}};

class PasswordChangeCredentialLeakDialogUtilsTest
    : public testing::TestWithParam<PasswordChangeParams> {
 public:
  PasswordChangeCredentialLeakDialogUtilsTest() = default;
};

TEST_P(PasswordChangeCredentialLeakDialogUtilsTest, ShouldShowCancelButton) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_show_cancel_button,
            ShouldShowCancelButton(GetParam().leak_type));
}

TEST_P(PasswordChangeCredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().accept_button_id),
            GetAcceptButtonLabel(GetParam().leak_type));
}

TEST_P(PasswordChangeCredentialLeakDialogUtilsTest, GetCancelButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  if (GetParam().should_show_cancel_button) {
    EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().cancel_button_id),
              GetCancelButtonLabel(GetParam().leak_type));
  }
}

INSTANTIATE_TEST_SUITE_P(InstantiationName,
                         PasswordChangeCredentialLeakDialogUtilsTest,
                         testing::ValuesIn(kPasswordChangeTestCases));
#endif
}  // namespace password_manager
