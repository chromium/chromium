// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

TEST(CredentialUIEntryTest, CredentialUIEntryWithForm) {
  std::u16string username = u"testUsername00";
  std::u16string password = u"testPassword01";

  PasswordForm form;
  form.app_display_name = "g.com";
  form.signon_realm = "https://g.com/";
  form.url = GURL(form.signon_realm);
  form.blocked_by_user = false;
  form.username_value = username;
  form.password_value = password;
  form.in_store = PasswordForm::Store::kProfileStore;

  CredentialUIEntry entry = CredentialUIEntry(form);

  unsigned long size = 1;
  EXPECT_EQ(entry.facets.size(), size);
  EXPECT_EQ(entry.facets[0].signon_realm, "https://g.com/");
  EXPECT_EQ(entry.stored_in.size(), size);
  EXPECT_EQ(entry.username, username);
  EXPECT_EQ(entry.password, password);
  EXPECT_EQ(entry.blocked_by_user, false);
}

TEST(CredentialUIEntryTest, CredentialUIEntryWithFormsVector) {
  std::vector<PasswordForm> forms;
  std::u16string username = u"testUsername00";
  std::u16string password = u"testPassword01";

  PasswordForm form;
  form.app_display_name = "g.com";
  form.signon_realm = "https://g.com/";
  form.url = GURL(form.signon_realm);
  form.blocked_by_user = false;
  form.username_value = username;
  form.password_value = password;
  form.in_store = PasswordForm::Store::kProfileStore;
  forms.push_back(std::move(form));

  PasswordForm form2;
  form2.app_display_name = "g2.com";
  form2.signon_realm = "https://g2.com/";
  form2.url = GURL(form2.signon_realm);
  form2.blocked_by_user = false;
  form2.username_value = username;
  form2.password_value = password;
  form2.in_store = PasswordForm::Store::kAccountStore;
  forms.push_back(std::move(form2));

  PasswordForm form3;
  form3.app_display_name = "g3.com";
  form3.signon_realm = "https://g3.com/";
  form3.url = GURL(form3.signon_realm);
  form3.blocked_by_user = false;
  form3.username_value = username;
  form3.password_value = password;
  form3.in_store = PasswordForm::Store::kAccountStore;
  forms.push_back(std::move(form3));

  CredentialUIEntry entry = CredentialUIEntry(forms);

  EXPECT_EQ(entry.facets.size(), forms.size());
  EXPECT_EQ(entry.facets[0].signon_realm, "https://g.com/");
  EXPECT_EQ(entry.facets[1].signon_realm, "https://g2.com/");
  EXPECT_EQ(entry.facets[2].signon_realm, "https://g3.com/");
  unsigned long stored_in_size = 2;
  EXPECT_EQ(entry.stored_in.size(), stored_in_size);
  EXPECT_EQ(entry.username, username);
  EXPECT_EQ(entry.password, password);
  EXPECT_EQ(entry.blocked_by_user, false);
}

}  // namespace password_manager
