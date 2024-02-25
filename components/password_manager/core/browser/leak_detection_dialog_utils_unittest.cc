// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

#include "base/i18n/message_formatter.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

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
const struct LeakTypeParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  // The rest of the fields specify what should be displayed for this test case.
  int accept_button_id;
  int cancel_button_id;
  int leak_message_id;
  int leak_title_id;
  bool should_show_cancel_button;
  bool should_check_passwords;
} kLeakTypesTestCases[] =
    {{CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), IDS_OK,
      IDS_CLOSE, GetLeakChangePasswordMessage(),
      IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
     {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
      IDS_CLOSE, GetLeakChangePasswordMessage(),
      IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
     {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
      IDS_CLOSE, GetLeakChangePasswordMessage(),
      IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
     {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
      IDS_CLOSE, GetLeakChangePasswordMessage(),
      IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false}},
  kPasswordCheckLeakTypesTestCases[] =
      {{CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
        IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED,
#else
        IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED,
#endif
        IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM, true, true},
       {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
        IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED,
#else
        IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED,
#endif
        IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM, true, true}
#if BUILDFLAG(IS_ANDROID)
},
  kPasswordCheckLeakTypesTestCasesAndroidAutomotive[] = {
      {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)), IDS_OK,
       IDS_CLOSE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
       IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED,
#else
       IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED,
#endif
       IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
      {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)), IDS_OK,
       IDS_CLOSE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
       IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED,
#else
       IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED,
#endif
       IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
#endif
};

struct BulkCheckParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  bool should_check_passwords;
} kBulkCheckTestCases[] =
    {
        {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)),
         false},
        {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)),
         false},
},
  kPasswordCheckBulkCheckTestCases[] =
      {{CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)), true},
       {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)), true},
       {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)), true}

#if BUILDFLAG(IS_ANDROID)
},
  kPasswordCheckBulkCheckTestCasesAndroidAutomotive[] = {
      {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)), false},
      {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)), false},
      {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)), false}
#endif
};
}  // namespace

class CredentialLeakDialogUtilsTest
    : public testing::TestWithParam<LeakTypeParams> {
 public:
  static std::vector<LeakTypeParams> GetTestCases() {
    std::vector<LeakTypeParams> test_cases;
    base::ranges::copy(kLeakTypesTestCases, std::back_inserter(test_cases));
#if BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      base::ranges::copy(kPasswordCheckLeakTypesTestCasesAndroidAutomotive,
                         std::back_inserter(test_cases));
      return test_cases;
    }
#endif
    base::ranges::copy(kPasswordCheckLeakTypesTestCases,
                       std::back_inserter(test_cases));
    return test_cases;
  }
};

TEST_P(CredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().accept_button_id),
            GetAcceptButtonLabel(GetParam().leak_type));
}

TEST_P(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetAcceptButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().accept_button_id),
            CreateDialogTraits(GetParam().leak_type)->GetAcceptButtonLabel());
}

TEST_P(CredentialLeakDialogUtilsTest, GetCancelButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().cancel_button_id),
            GetCancelButtonLabel(GetParam().leak_type));
}

TEST_P(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetCancelButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().cancel_button_id),
            CreateDialogTraits(GetParam().leak_type)->GetCancelButtonLabel());
}

TEST_P(CredentialLeakDialogUtilsTest, GetDescription) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  std::u16string expected_message =
      l10n_util::GetStringUTF16(GetParam().leak_message_id);
  EXPECT_EQ(expected_message, GetDescription(GetParam().leak_type));
}

TEST_P(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetDescription) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().leak_message_id),
            CreateDialogTraits(GetParam().leak_type)->GetDescription());
}

TEST_P(CredentialLeakDialogUtilsTest, GetTitle) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().leak_title_id),
            GetTitle(GetParam().leak_type));
}

TEST_P(CredentialLeakDialogUtilsTest, LeakDialogTraits_GetTitle) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().leak_title_id),
            CreateDialogTraits(GetParam().leak_type)->GetTitle());
}

TEST_P(CredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_check_passwords,
            ShouldCheckPasswords(GetParam().leak_type));
}

TEST_P(CredentialLeakDialogUtilsTest, LeakDialogTraits_ShouldCheckPasswords) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_check_passwords,
            CreateDialogTraits(GetParam().leak_type)->ShouldCheckPasswords());
}

TEST_P(CredentialLeakDialogUtilsTest, ShouldShowCancelButton) {
  EXPECT_EQ(GetParam().should_show_cancel_button,
            ShouldShowCancelButton(GetParam().leak_type));
}

TEST_P(CredentialLeakDialogUtilsTest, LeakDialogTraits_ShouldShowCancelButton) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_show_cancel_button,
            CreateDialogTraits(GetParam().leak_type)->ShouldShowCancelButton());
}

INSTANTIATE_TEST_SUITE_P(
    InstantiationName,
    CredentialLeakDialogUtilsTest,
    testing::ValuesIn(CredentialLeakDialogUtilsTest::GetTestCases()));

class BulkCheckCredentialLeakDialogUtilsTest
    : public testing::TestWithParam<BulkCheckParams> {
 public:
  static std::vector<BulkCheckParams> GetTestCases() {
    std::vector<BulkCheckParams> test_cases;
    base::ranges::copy(kBulkCheckTestCases, std::back_inserter(test_cases));
#if BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      base::ranges::copy(kPasswordCheckBulkCheckTestCasesAndroidAutomotive,
                         std::back_inserter(test_cases));
      return test_cases;
    }
#endif
    base::ranges::copy(kPasswordCheckBulkCheckTestCases,
                       std::back_inserter(test_cases));
    return test_cases;
  }
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

INSTANTIATE_TEST_SUITE_P(
    InstantiationName,
    BulkCheckCredentialLeakDialogUtilsTest,
    testing::ValuesIn(BulkCheckCredentialLeakDialogUtilsTest::GetTestCases()));

#if BUILDFLAG(IS_ANDROID)
struct PasswordChangeParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  // The rest of the fields specify what should be displayed for this test case.
  int accept_button_id;
  int cancel_button_id;
  bool should_show_cancel_button;
  bool should_show_change_password_button;
} kPasswordChangeTestCases[] =
    {{CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), IDS_OK,
      0, false, false},
     {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
      0, false, false},
     {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)), IDS_OK,
      0, false, false}},
  kPasswordChangeTestCasesNonAuto[] =
      {{CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)),
        IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, false},
       {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
        IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, false},
       {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
        IDS_CLOSE, false, true},
       {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)),
        IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, false},
       {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
        IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE, true, true}},
  kPasswordChangeTestCasesAuto[] = {
      {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)), IDS_OK,
       0, false, false},
      {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)), IDS_OK,
       0, false, false},
      {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
       0, false, false},
      {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)), IDS_OK,
       0, false, false},
      {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)), IDS_OK,
       0, false, false}};

class PasswordChangeCredentialLeakDialogUtilsTest
    : public testing::TestWithParam<PasswordChangeParams> {
 public:
  PasswordChangeCredentialLeakDialogUtilsTest() = default;

  static std::vector<PasswordChangeParams> GetTestCases() {
    std::vector<PasswordChangeParams> test_cases;
    base::ranges::copy(kPasswordChangeTestCases,
                       std::back_inserter(test_cases));

    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      base::ranges::copy(kPasswordChangeTestCasesAuto,
                         std::back_inserter(test_cases));
      return test_cases;
    }

    base::ranges::copy(kPasswordChangeTestCasesNonAuto,
                       std::back_inserter(test_cases));
    return test_cases;
  }
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

INSTANTIATE_TEST_SUITE_P(
    InstantiationName,
    PasswordChangeCredentialLeakDialogUtilsTest,
    testing::ValuesIn(
        PasswordChangeCredentialLeakDialogUtilsTest::GetTestCases()));
#endif
}  // namespace password_manager
