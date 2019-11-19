// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "base/strings/utf_string_conversions.h"
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

namespace password_manager {

namespace {

// Contains information that should be displayed on the leak dialog for
// specified |leak_type|.
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
     IDS_CLOSE, IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE, false, false},
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE, false, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
     IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE, true, true},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE, false, false},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
     IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE, IDS_CREDENTIAL_LEAK_TITLE,
     true, true}};
}  // namespace

TEST(CredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].accept_button_id),
        GetAcceptButtonLabel(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, GetCancelButtonLabel) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].cancel_button_id),
        GetCancelButtonLabel());
  }
}

TEST(CredentialLeakDialogUtilsTest, GetCheckPasswordsDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    if (kLeakTypesTestCases[i].leak_message_id ==
        IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE) {
      SCOPED_TRACE(testing::Message() << i);
      base::string16 expected_message = l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE);
      EXPECT_EQ(expected_message,
                GetDescription(kLeakTypesTestCases[i].leak_type, origin));
    }
  }
}

TEST(CredentialLeakDialogUtilsTest, GetChangeAndCheckPasswordsDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    if (kLeakTypesTestCases[i].leak_message_id ==
        IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE) {
      SCOPED_TRACE(testing::Message() << i);
      base::string16 expected_message = l10n_util::GetStringFUTF16(
          IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE,
          url_formatter::FormatOriginForSecurityDisplay(
              url::Origin::Create(origin),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
      EXPECT_EQ(expected_message,
                GetDescription(kLeakTypesTestCases[i].leak_type, origin));
    }
  }
}

TEST(CredentialLeakDialogUtilsTest, GetChangePasswordDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    if (kLeakTypesTestCases[i].leak_message_id ==
        IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE) {
      SCOPED_TRACE(testing::Message() << i);
      base::string16 expected_message = l10n_util::GetStringFUTF16(
          IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
          url_formatter::FormatOriginForSecurityDisplay(
              url::Origin::Create(origin),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
      EXPECT_EQ(expected_message,
                GetDescription(kLeakTypesTestCases[i].leak_type, origin));
    }
  }
}

TEST(CredentialLeakDialogUtilsTest, GetTitle) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_title_id),
              GetTitle(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_check_passwords,
              ShouldCheckPasswords(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, ShouldShowCancelButton) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_show_cancel_button,
              ShouldShowCancelButton(kLeakTypesTestCases[i].leak_type));
  }
}

}  // namespace password_manager
