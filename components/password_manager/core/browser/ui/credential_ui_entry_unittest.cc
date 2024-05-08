// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::UnorderedElementsAre;

constexpr char kTestCom[] = "https://test.com";
constexpr char kTestComChangePassword[] =
    "https://test.com/.well-known/change-password";
constexpr char kAndroidSignonRealm[] =
    "android://certificate_hash@com.test.client/";

// Creates matcher for a given domain info
auto ExpectDomain(const std::string& name, const GURL& url) {
  return AllOf(testing::Field(&CredentialUIEntry::DomainInfo::name, name),
               testing::Field(&CredentialUIEntry::DomainInfo::url, url));
}

// Creates a CredentialUIEntry with the given insecurity issue.
CredentialUIEntry CreateInsecureCredential(InsecureType insecure_type) {
  base::flat_map<InsecureType, InsecurityMetadata> form_issues;
  form_issues[insecure_type] = InsecurityMetadata();

  PasswordForm form;
  form.password_issues = form_issues;

  return CredentialUIEntry(form);
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
  EXPECT_TRUE(entry.passkey_credential_id.empty());
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

TEST(CredentialUIEntryTest, CredentialUIEntryFromPasskey) {
  const std::vector<uint8_t> cred_id = {1, 2, 3, 4};
  const std::vector<uint8_t> user_id = {5, 6, 7, 4};
  const std::u16string kUsername = u"marisa";
  const std::u16string kDisplayName = u"Marisa Kirisame";
  PasskeyCredential passkey(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId("rpid.com"),
      PasskeyCredential::CredentialId(cred_id),
      PasskeyCredential::UserId(user_id),
      PasskeyCredential::Username(base::UTF16ToUTF8(kUsername)),
      PasskeyCredential::DisplayName(base::UTF16ToUTF8(kDisplayName)));
  CredentialUIEntry entry(passkey);
  EXPECT_EQ(entry.passkey_credential_id, cred_id);
  EXPECT_EQ(entry.username, kUsername);
  EXPECT_EQ(entry.user_display_name, kDisplayName);
  ASSERT_EQ(entry.facets.size(), 1u);
  EXPECT_EQ(entry.facets.at(0).url, GURL("https://rpid.com/"));
  EXPECT_EQ(entry.facets.at(0).signon_realm, "https://rpid.com");
  EXPECT_TRUE(entry.stored_in.empty());
}

TEST(CredentialUIEntryTest, TestGetAffiliatedDomains) {
  std::vector<PasswordForm> forms;

  PasswordForm android_form;
  android_form.signon_realm = kAndroidSignonRealm;
  android_form.app_display_name = "g3.com";
  android_form.affiliated_web_realm = kTestCom;

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
  android_form.signon_realm = kAndroidSignonRealm;

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

TEST(CredentialUIEntryTest, CredentialUIEntryInsecureHelpers) {
  PasswordForm form;
  auto entry = CredentialUIEntry(form);

  EXPECT_FALSE(entry.IsLeaked());
  EXPECT_FALSE(entry.IsPhished());
  EXPECT_FALSE(entry.IsWeak());
  EXPECT_FALSE(entry.IsReused());

  auto leaked_entry = CreateInsecureCredential(InsecureType::kLeaked);
  EXPECT_TRUE(leaked_entry.IsLeaked());
  EXPECT_TRUE(IsCompromised(leaked_entry));

  auto phished_entry = CreateInsecureCredential(InsecureType::kPhished);
  EXPECT_TRUE(phished_entry.IsPhished());
  EXPECT_TRUE(IsCompromised(phished_entry));

  auto weak_entry = CreateInsecureCredential(InsecureType::kWeak);
  EXPECT_TRUE(weak_entry.IsWeak());

  auto reused_entry = CreateInsecureCredential(InsecureType::kReused);
  EXPECT_TRUE(reused_entry.IsReused());
}

TEST(CredentialUIEntryTest, TestGetAffiliatedDomainsWithDuplicates) {
  PasswordForm form1;
  form1.signon_realm = "https://g.com/";
  form1.url = GURL("https://g.com/");

  PasswordForm form2;
  form2.signon_realm = "https://g.com/";
  form2.url = GURL("https://g.com/");

  CredentialUIEntry entry = CredentialUIEntry({form1, form2});
  EXPECT_THAT(entry.GetAffiliatedDomains(),
              ElementsAre(ExpectDomain("g.com", form1.url)));
}

TEST(CredentialUIEntryTest, TestGetAffiliatedDuplicatesWithDifferentUrls) {
  PasswordForm form1;
  form1.signon_realm = "https://g.com/";
  form1.url = GURL("https://g.com/login/");

  PasswordForm form2;
  form2.signon_realm = "https://g.com/";
  form2.url = GURL("https://g.com/sign%20in/");

  CredentialUIEntry entry = CredentialUIEntry({form1, form2});
  EXPECT_THAT(entry.GetAffiliatedDomains(),
              UnorderedElementsAre(ExpectDomain("g.com", form1.url),
                                   ExpectDomain("g.com", form2.url)));
}

TEST(CredentialUIEntryTest, TestGetInvalidAffiliatedDomains) {
  PasswordForm form;
  form.signon_realm = "htt://g.com/";
  form.url = GURL("htt://g.com/login/");

  CredentialUIEntry entry = CredentialUIEntry({form});
  EXPECT_THAT(entry.GetAffiliatedDomains(),
              ElementsAre(ExpectDomain("htt://g.com/login/", form.url)));
}

TEST(CredentialUIEntryTest, TestGetChangeURLAndroid) {
  PasswordForm android_form;
  android_form.signon_realm = kAndroidSignonRealm;
  android_form.affiliated_web_realm = kTestCom;
  CredentialUIEntry entry = CredentialUIEntry(android_form);
  EXPECT_EQ(entry.GetChangePasswordURL(), GURL(kTestComChangePassword));
}

TEST(CredentialUIEntryTest, TestGetChangeURLAndroidNoAffiliatedWebRealm) {
  PasswordForm android_form;
  android_form.signon_realm = kAndroidSignonRealm;
  CredentialUIEntry entry = CredentialUIEntry(android_form);
  EXPECT_FALSE(entry.GetChangePasswordURL());
}

TEST(CredentialUIEntryTest, TestGetChangeURLWebForm) {
  PasswordForm web_form;
  web_form.url = GURL(kTestCom);
  CredentialUIEntry entry = CredentialUIEntry(web_form);
  EXPECT_EQ(entry.GetChangePasswordURL(), GURL(kTestComChangePassword));
}

TEST(CredentialUIEntryTest, EntriesDifferingByStoreShouldMapToSameKey) {
  PasswordForm account_form;
  account_form.signon_realm = "https://g.com/";
  account_form.url = GURL(account_form.signon_realm);
  account_form.blocked_by_user = false;
  account_form.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_form(account_form);
  profile_form.in_store = PasswordForm::Store::kProfileStore;

  EXPECT_EQ(CreateSortKey(CredentialUIEntry(account_form)),
            CreateSortKey(CredentialUIEntry(profile_form)));
}

TEST(CredentialUIEntryTest, PasskeyVsPasswordSortKey) {
  PasswordForm form;
  form.signon_realm = "https://test.com/";
  form.url = GURL(form.signon_realm);
  form.username_value = u"victor";
  CredentialUIEntry password(std::move(form));

  PasskeyCredential passkey_credential(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId("test.com"),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId(), PasskeyCredential::Username("victor"));
  CredentialUIEntry passkey(std::move(passkey_credential));

  EXPECT_NE(CreateSortKey(password), CreateSortKey(passkey));
}

// Tests that two passkeys that are equal in everything but the display name
// have different sort keys.
TEST(CredentialUIEntryTest, PasskeyDifferentSortKeyForDifferentDisplayName) {
  PasskeyCredential passkey_credential(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId("test.com"),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId(), PasskeyCredential::Username("victor"),
      PasskeyCredential::DisplayName("Display Name 1"));
  CredentialUIEntry passkey1(std::move(passkey_credential));
  CredentialUIEntry passkey2 = passkey1;
  passkey2.user_display_name = u"Display Name 2";

  EXPECT_NE(CreateSortKey(passkey1), CreateSortKey(passkey2));
}

}  // namespace password_manager
