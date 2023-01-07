// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;

namespace password_manager {

namespace {

MATCHER_P3(FormHasOriginUsernamePassword, origin, username, password, "") {
  return arg.GetFirstSignonRealm() == origin && arg.GetURL() == GURL(origin) &&
         arg.username == base::UTF8ToUTF16(username) &&
         arg.password == base::UTF8ToUTF16(password);
}

}  // namespace

TEST(PasswordCSVWriterTest, SerializePasswords_ZeroPasswords) {
  std::vector<CredentialUIEntry> credentials;

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(credentials));
  ASSERT_EQ(CSVPassword::Status::kOK, seq.result());

  EXPECT_EQ(seq.begin(), seq.end());
}

TEST(PasswordCSVWriterTest, SerializePasswords_SinglePassword) {
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form;
  form.url = GURL("http://example.com");
  form.username_value = u"Someone";
  form.password_value = u"Secret";
  credentials.emplace_back(form);

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(credentials));
  ASSERT_EQ(CSVPassword::Status::kOK, seq.result());

  std::vector<CredentialUIEntry> pwds;
  for (const auto& pwd : seq) {
    pwds.emplace_back(pwd);
  }
  EXPECT_THAT(pwds, ElementsAre(FormHasOriginUsernamePassword(
                        "http://example.com/", "Someone", "Secret")));
}

TEST(PasswordCSVWriterTest, SerializePasswords_TwoPasswords) {
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form;
  form.url = GURL("http://example.com");
  form.username_value = u"Someone";
  form.password_value = u"Secret";
  credentials.emplace_back(form);
  form.url = GURL("http://other.org");
  form.username_value = u"Anyone";
  form.password_value = u"None";
  credentials.emplace_back(form);

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(credentials));
  ASSERT_EQ(CSVPassword::Status::kOK, seq.result());

  std::vector<CredentialUIEntry> pwds;
  for (const auto& pwd : seq) {
    pwds.emplace_back(pwd);
  }
  EXPECT_THAT(pwds, ElementsAre(FormHasOriginUsernamePassword(
                                    "http://example.com/", "Someone", "Secret"),
                                FormHasOriginUsernamePassword(
                                    "http://other.org/", "Anyone", "None")));
}

}  // namespace password_manager
