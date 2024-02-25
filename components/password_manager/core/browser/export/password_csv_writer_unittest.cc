// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;

#if BUILDFLAG(IS_WIN)
const std::string kLineEnding = "\r\n";
#else
const std::string kLineEnding = "\n";
#endif

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
  const std::u16string kNoteValue =
      base::UTF8ToUTF16("Note Line 1" + kLineEnding + "Note Line 2");
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"Someone";
  form.password_value = u"Secret";
  form.notes = {PasswordNote(kNoteValue, base::Time::Now())};
  credentials.emplace_back(form);

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(credentials));
  ASSERT_EQ(seq.result(), CSVPassword::Status::kOK);

  std::vector<CredentialUIEntry> pwds;
  for (const auto& pwd : seq) {
    pwds.emplace_back(pwd);
  }
  EXPECT_THAT(pwds, ElementsAre(FormHasOriginUsernamePassword(
                        "https://example.com/", "Someone", "Secret")));

  std::string expected =
      "name,url,username,password,note" + kLineEnding +
      "example.com,https://example.com/,Someone,Secret,\"Note Line "
      "1" +
      kLineEnding + "Note Line 2\"" + kLineEnding;
  EXPECT_EQ(PasswordCSVWriter::SerializePasswords(credentials), expected);
}

TEST(PasswordCSVWriterTest, SerializePasswords_TwoPasswords) {
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"Someone";
  form.password_value = u"Secret";
  credentials.emplace_back(form);
  form.signon_realm = "https://other.org/";
  form.url = GURL("https://other.org");
  form.username_value = u"Anyone";
  form.password_value = u"None";
  credentials.emplace_back(form);

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(credentials));
  ASSERT_EQ(seq.result(), CSVPassword::Status::kOK);

  std::vector<CredentialUIEntry> pwds;
  for (const auto& pwd : seq) {
    pwds.emplace_back(pwd);
  }
  EXPECT_THAT(pwds,
              ElementsAre(FormHasOriginUsernamePassword("https://example.com/",
                                                        "Someone", "Secret"),
                          FormHasOriginUsernamePassword("https://other.org/",
                                                        "Anyone", "None")));
}

TEST(PasswordCSVWriterTest, SerializePasswordsWritesNames) {
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"a";
  form.password_value = u"b";
  credentials.emplace_back(form);
  form.url = GURL(
      "android://"
      "Jzj5T2E45Hb33D-lk-"
      "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsBzC09Gk094Z-U_hg=="
      "@"
      "com.netflix.mediaclient");
  form.signon_realm =
      "android://"
      "Jzj5T2E45Hb33D-lk-"
      "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsBzC09Gk094Z-U_hg=="
      "@"
      "com.netflix.mediaclient";
  form.app_display_name = "Netflix";
  form.username_value = u"a";
  form.password_value = u"b";
  credentials.emplace_back(form);
  std::string expected = "name,url,username,password,note" + kLineEnding +
                         "Netflix,android://Jzj5T2E45Hb33D-lk-"
                         "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsB"
                         "zC09Gk094Z-U_hg==@com.netflix.mediaclient,a,b," +
                         kLineEnding + "example.com,https://example.com/,a,b," +
                         kLineEnding;
  EXPECT_EQ(PasswordCSVWriter::SerializePasswords(credentials), expected);
}

TEST(PasswordCSVWriterTest, SerializePasswordsIsSorted) {
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"a";
  form.password_value = u"b";
  credentials.emplace_back(form);
  form.signon_realm = "https://other.org/";
  form.url = GURL("https://other.org");
  form.username_value = u"a";
  form.password_value = u"b";
  credentials.emplace_back(form);
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"someone";
  form.password_value = u"secret";
  credentials.emplace_back(form);
  form.signon_realm = "https://example.org/";
  form.url = GURL("https://example.org");
  form.username_value = u"a";
  form.password_value = u"b";
  credentials.emplace_back(form);
  std::string expected = "name,url,username,password,note" + kLineEnding +
                         "example.com,https://example.com/,a,b," + kLineEnding +
                         "example.com,https://example.com/,someone,secret," +
                         kLineEnding + "example.org,https://example.org/,a,b," +
                         kLineEnding + "other.org,https://other.org/,a,b," +
                         kLineEnding;
  EXPECT_EQ(PasswordCSVWriter::SerializePasswords(credentials), expected);
}

TEST(PasswordCSVWriterTest, SerializeAffiliatedPasswords) {
  std::vector<CredentialUIEntry> credentials;
  PasswordForm form1;
  form1.signon_realm = "https://example.com/";
  form1.url = GURL("https://example.com");
  form1.username_value = u"username";
  form1.password_value = u"Secret";
  PasswordForm form2;
  form2.signon_realm = "https://other.org/";
  form2.url = GURL("https://other.org");
  form2.username_value = form1.username_value;
  form2.password_value = form1.password_value;
  credentials.emplace_back(std::vector<PasswordForm>{form1, form2});

  CSVPasswordSequence seq(PasswordCSVWriter::SerializePasswords(credentials));
  ASSERT_EQ(seq.result(), CSVPassword::Status::kOK);

  std::vector<CredentialUIEntry> pwds;
  for (const auto& pwd : seq) {
    pwds.emplace_back(pwd);
  }
  EXPECT_THAT(pwds,
              ElementsAre(FormHasOriginUsernamePassword("https://example.com/",
                                                        "username", "Secret"),
                          FormHasOriginUsernamePassword("https://other.org/",
                                                        "username", "Secret")));
}

}  // namespace password_manager
