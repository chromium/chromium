// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credentials_cleaner.h"

#include <string>
#include <utility>

#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
using ::testing::Pointee;

namespace password_manager {

TEST(CredentialsCleaner, RemoveNonHTTPOrHTTPSForms) {
  std::vector<std::unique_ptr<PasswordForm>> forms;

  PasswordForm http_form;
  http_form.signon_realm = "http://example.com";
  forms.push_back(std::make_unique<PasswordForm>(http_form));

  PasswordForm https_form;
  https_form.signon_realm = "https://example.com";
  forms.push_back(std::make_unique<PasswordForm>(https_form));

  PasswordForm federated_form;
  federated_form.signon_realm = "federation://example.com/google.com";
  forms.push_back(std::make_unique<PasswordForm>(federated_form));

  PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.beta.android";
  forms.push_back(std::make_unique<PasswordForm>(android_form));

  PasswordForm basic_auth_form;
  basic_auth_form.signon_realm =
      "https://example.com/`My` |Realm| %With% <Special> {Chars}\r\t\n\x01";
  forms.push_back(std::make_unique<PasswordForm>(basic_auth_form));
  EXPECT_TRUE(GURL(basic_auth_form.signon_realm).is_valid());

  // Note: We are using std::string_literals to be able to construct a
  // std::string with an embedded null character.
  using std::string_literals::operator""s;
  PasswordForm basic_auth_form_with_null_character;
  basic_auth_form_with_null_character.signon_realm =
      "http://example.com/valid\0Realm"s;
  forms.push_back(
      std::make_unique<PasswordForm>(basic_auth_form_with_null_character));
  EXPECT_TRUE(
      GURL(basic_auth_form_with_null_character.signon_realm).is_valid());

  PasswordForm invalid_basic_auth_form;
  invalid_basic_auth_form.signon_realm = "http://example[invalid].com/";
  forms.push_back(std::make_unique<PasswordForm>(invalid_basic_auth_form));
  EXPECT_FALSE(GURL(invalid_basic_auth_form.signon_realm).is_valid());

  PasswordForm another_invalid_form;
  another_invalid_form.signon_realm = "http://";
  forms.push_back(std::make_unique<PasswordForm>(another_invalid_form));
  EXPECT_FALSE(GURL(another_invalid_form.signon_realm).is_valid());

  // Expect that only the federated and Android form got removed.
  EXPECT_THAT(CredentialsCleaner::RemoveNonHTTPOrHTTPSForms(std::move(forms)),
              ElementsAre(Pointee(http_form), Pointee(https_form),
                          Pointee(basic_auth_form),
                          Pointee(basic_auth_form_with_null_character),
                          Pointee(invalid_basic_auth_form),
                          Pointee(another_invalid_form)));
}

}  // namespace password_manager
