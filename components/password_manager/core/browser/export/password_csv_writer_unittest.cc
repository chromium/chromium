// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;

namespace password_manager {

namespace {

MATCHER_P3(FormHasOriginUsernamePassword, origin, username, password, "") {
  return arg.signon_realm == origin && arg.url == GURL(origin) &&
         arg.username_value == base::UTF8ToUTF16(username) &&
         arg.password_value == base::UTF8ToUTF16(password);
}

}  // namespace

TEST(PasswordCSVWriterTest, SerializePasswords_ZeroPasswords) {
  std::vector<std::unique_ptr<PasswordForm>> passwords;

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(passwords));
  ASSERT_EQ(CSVPassword::Status::kOK, seq.result());

  EXPECT_EQ(seq.begin(), seq.end());
}

TEST(PasswordCSVWriterTest, SerializePasswords_SinglePassword) {
  std::vector<std::unique_ptr<PasswordForm>> passwords;
  PasswordForm form;
  form.url = GURL("http://example.com");
  form.username_value = base::UTF8ToUTF16("Someone");
  form.password_value = base::UTF8ToUTF16("Secret");
  passwords.push_back(std::make_unique<PasswordForm>(form));

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(passwords));
  ASSERT_EQ(CSVPassword::Status::kOK, seq.result());

  std::vector<PasswordForm> pwds;
  for (const auto& pwd : seq) {
    pwds.push_back(pwd.ParseValid());
  }
  EXPECT_THAT(pwds, ElementsAre(FormHasOriginUsernamePassword(
                        "http://example.com/", "Someone", "Secret")));
}

TEST(PasswordCSVWriterTest, SerializePasswords_TwoPasswords) {
  std::vector<std::unique_ptr<PasswordForm>> passwords;
  PasswordForm form;
  form.url = GURL("http://example.com");
  form.username_value = base::UTF8ToUTF16("Someone");
  form.password_value = base::UTF8ToUTF16("Secret");
  passwords.push_back(std::make_unique<PasswordForm>(form));
  form.url = GURL("http://other.org");
  form.username_value = base::UTF8ToUTF16("Anyone");
  form.password_value = base::UTF8ToUTF16("None");
  passwords.push_back(std::make_unique<PasswordForm>(form));

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(passwords));
  ASSERT_EQ(CSVPassword::Status::kOK, seq.result());

  std::vector<PasswordForm> pwds;
  for (const auto& pwd : seq) {
    pwds.push_back(pwd.ParseValid());
  }
  EXPECT_THAT(pwds, ElementsAre(FormHasOriginUsernamePassword(
                                    "http://example.com/", "Someone", "Secret"),
                                FormHasOriginUsernamePassword(
                                    "http://other.org/", "Anyone", "None")));
}

}  // namespace password_manager
