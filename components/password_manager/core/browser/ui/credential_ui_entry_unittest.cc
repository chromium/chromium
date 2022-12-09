// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::UnorderedElementsAre;

// Creates matcher for a given domain info
auto ExpectDomain(const std::string& name, const GURL& url) {
  return AllOf(testing::Field(&CredentialUIEntry::DomainInfo::name, name),
               testing::Field(&CredentialUIEntry::DomainInfo::url, url));
}

}  // namespace

TEST(CredentialUIEntryTest, CredentialUIEntryFromForm) {
  const std::u16string kUsername = u"testUsername00";
  const std::u16string kPassword = u"testPassword01";

  PasswordForm form;
  form.app_display_name = "g.com";
  form.signon_realm = "https://g.com/";
  form.url = GURL(form.signon_realm);
  form.blocked_by_user = false;
  form.username_value = kUsername;
  form.password_value = kPassword;
  form.in_store = PasswordForm::Store::kProfileStore;

  CredentialUIEntry entry = CredentialUIEntry(form);

  unsigned long size = 1;
  EXPECT_EQ(entry.facets.size(), size);
  EXPECT_EQ(entry.facets[0].signon_realm, "https://g.com/");
  EXPECT_EQ(entry.stored_in.size(), size);
  EXPECT_EQ(entry.username, kUsername);
  EXPECT_EQ(entry.password, kPassword);
  EXPECT_EQ(entry.blocked_by_user, false);
}

TEST(CredentialUIEntryTest,
     CredentialUIEntryFromFormsVectorWithIdenticalNotes) {
  std::vector<PasswordForm> forms;
  const std::u16string kUsername = u"testUsername00";
  const std::u16string kPassword = u"testPassword01";
  const std::u16string kNote = u"Test New Note \n";

  PasswordForm form;
  form.app_display_name = "g.com";
  form.signon_realm = "https://g.com/";
  form.url = GURL(form.signon_realm);
  form.blocked_by_user = false;
  form.username_value = kUsername;
  form.password_value = kPassword;
  form.SetNoteWithEmptyUniqueDisplayName(kNote);
  form.in_store = PasswordForm::Store::kProfileStore;
  forms.push_back(std::move(form));

  PasswordForm form2;
  form2.app_display_name = "g2.com";
  form2.signon_realm = "https://g2.com/";
  form2.url = GURL(form2.signon_realm);
  form2.blocked_by_user = false;
  form2.username_value = kUsername;
  form2.password_value = kPassword;
  form2.SetNoteWithEmptyUniqueDisplayName(kNote);
  form2.in_store = PasswordForm::Store::kAccountStore;
  forms.push_back(std::move(form2));

  PasswordForm form3;
  form3.app_display_name = "g3.com";
  form3.signon_realm = "https://g3.com/";
  form3.url = GURL(form3.signon_realm);
  form3.blocked_by_user = false;
  form3.username_value = kUsername;
  form3.password_value = kPassword;
  form3.in_store = PasswordForm::Store::kAccountStore;
  forms.push_back(std::move(form3));

  CredentialUIEntry entry = CredentialUIEntry(forms);

  EXPECT_EQ(entry.facets.size(), forms.size());
  EXPECT_EQ(entry.facets[0].signon_realm, "https://g.com/");
  EXPECT_EQ(entry.facets[1].signon_realm, "https://g2.com/");
  EXPECT_EQ(entry.facets[2].signon_realm, "https://g3.com/");
  unsigned long stored_in_size = 2;
  EXPECT_EQ(entry.stored_in.size(), stored_in_size);
  EXPECT_EQ(entry.username, kUsername);
  EXPECT_EQ(entry.password, kPassword);
  EXPECT_EQ(entry.note, kNote);
  EXPECT_EQ(entry.blocked_by_user, false);
}

TEST(CredentialUIEntryTest, TestGetAffiliatedDomains) {
  std::vector<PasswordForm> forms;

  PasswordForm android_form;
  android_form.signon_realm = "android://certificate_hash@com.test.client/";
  android_form.app_display_name = "g3.com";
  android_form.affiliated_web_realm = "https://test.com";

  PasswordForm web_form;
  web_form.signon_realm = "https://g.com/";
  web_form.url = GURL(web_form.signon_realm);

  CredentialUIEntry entry = CredentialUIEntry({android_form, web_form});
  EXPECT_THAT(entry.GetAffiliatedDomains(),
              UnorderedElementsAre(
                  ExpectDomain(android_form.app_display_name,
                               GURL(android_form.affiliated_web_realm)),
                  ExpectDomain("g.com", web_form.url)));
}

TEST(CredentialUIEntryTest, TestGetAffiliatedDomainsHttpForm) {
  PasswordForm form;
  form.signon_realm = "http://g.com/";
  form.url = GURL(form.signon_realm);

  CredentialUIEntry entry = CredentialUIEntry({form});
  EXPECT_THAT(entry.GetAffiliatedDomains(),
              ElementsAre(ExpectDomain("http://g.com", GURL(form.url))));
}

TEST(CredentialUIEntryTest, TestGetAffiliatedDomainsEmptyAndroidForm) {
  PasswordForm android_form;
  android_form.signon_realm = "android://certificate_hash@com.test.client/";

  CredentialUIEntry entry = CredentialUIEntry({android_form});
  EXPECT_THAT(entry.GetAffiliatedDomains(),
              ElementsAre(ExpectDomain(
                  "client.test.com", GURL("https://play.google.com/store/apps/"
                                          "details?id=com.test.client"))));
}

TEST(CredentialUIEntryTest,
     CredentialUIEntryFromFormsVectorWithDifferentNotes) {
  std::vector<PasswordForm> forms;
  const std::u16string kNotes[] = {u"Note", u"", u"Another note"};

  for (const auto& kNote : kNotes) {
    PasswordForm form;
    form.signon_realm = "https://g.com/";
    form.url = GURL(form.signon_realm);
    form.password_value = u"pwd";
    form.SetNoteWithEmptyUniqueDisplayName(kNote);
    forms.push_back(std::move(form));
  }

  CredentialUIEntry entry = CredentialUIEntry(forms);

  // Notes are concatenated alphabetically.
  EXPECT_EQ(entry.note, kNotes[2] + u"\n" + kNotes[0]);
}

}  // namespace password_manager
