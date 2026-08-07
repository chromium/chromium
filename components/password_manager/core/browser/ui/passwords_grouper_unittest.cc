// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include <functional>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/test/gmock_callback_support.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using ::affiliations::Facet;
using ::affiliations::FacetURI;
using ::affiliations::GroupedFacets;
using ::affiliations::MockAffiliationService;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::UnorderedElementsAre;

PasskeyCredential CreatePasskey(std::string rp_id,
                                std::string username = "username",
                                std::string display_name = "display_name") {
  return PasskeyCredential(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId(std::move(rp_id)),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId({5, 6, 7, 8}),
      PasskeyCredential::Username(std::move(username)),
      PasskeyCredential::DisplayName(std::move(display_name)));
}

StoredCredential CreateStoredCredential(std::string signon_realm,
                                        std::u16string username = u"username",
                                        std::u16string password = u"password") {
  StoredCredential cred;
  cred.signon_realm = signon_realm;
  cred.username_value = username;
  cred.password_value = PasswordString(std::move(password));
  cred.url = GURL(signon_realm);
  return cred;
}

StoredCredential CloneStoredCredential(const StoredCredential& cred) {
  StoredCredential copy;
  copy.primary_key = cred.primary_key;
  copy.scheme = cred.scheme;
  copy.signon_realm = cred.signon_realm;
  copy.url = cred.url;
  copy.action = cred.action;
  copy.federation_origin = cred.federation_origin;
  copy.change_password_url = cred.change_password_url;
  copy.submit_element = cred.submit_element;
  copy.username_element = cred.username_element;
  copy.password_element = cred.password_element;
  copy.username_value = cred.username_value;
  copy.password_value = PasswordString(cred.password_value.value());
  copy.all_alternative_usernames = cred.all_alternative_usernames;
  copy.date_created = cred.date_created;
  copy.date_last_used = cred.date_last_used;
  copy.date_last_filled = cred.date_last_filled;
  copy.date_password_modified = cred.date_password_modified;
  copy.date_received = cred.date_received;
  copy.blocked_by_user = cred.blocked_by_user;
  copy.type = cred.type;
  copy.times_used_in_html_form = cred.times_used_in_html_form;
  copy.affiliated_web_realm = cred.affiliated_web_realm;
  copy.display_name = cred.display_name;
  copy.icon_url = cred.icon_url;
  copy.app_display_name = cred.app_display_name;
  copy.app_icon_url = cred.app_icon_url;
  copy.previously_associated_sync_account_email =
      cred.previously_associated_sync_account_email;
  copy.match_type = cred.match_type;
  copy.skip_zero_click = cred.skip_zero_click;
  copy.generation_upload_status = cred.generation_upload_status;
  copy.in_store = cred.in_store;
  copy.moving_blocked_for_list = cred.moving_blocked_for_list;
  copy.password_issues = cred.password_issues;
  copy.notes = cred.notes;
  copy.form_data = cred.form_data;
  copy.keychain_identifier = cred.keychain_identifier;
  copy.sender_email = cred.sender_email;
  copy.sender_name = cred.sender_name;
  copy.sharing_notification_displayed = cred.sharing_notification_displayed;
  copy.sender_profile_image_url = cred.sender_profile_image_url;
  copy.actor_login_approved = cred.actor_login_approved;
  return copy;
}

template <typename... Args>
std::vector<StoredCredential> MakeStoredCredentials(Args&&... args) {
  std::vector<StoredCredential> result;
  result.reserve(sizeof...(args));
  (result.push_back(std::forward<Args>(args)), ...);
  return result;
}

GroupedFacets GetSingleGroupForCredential(const StoredCredential& cred) {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(cred.signon_realm))};
  return group;
}

GURL GetIconUrl(const std::string& site) {
  GURL::Replacements replacements;
  std::string query =
      "client=PASSWORD_MANAGER&type=FAVICON&fallback_opts=TYPE,SIZE,URL,TOP_"
      "DOMAIN&size=32&url=" +
      base::EscapeQueryParamValue(site,
                                  /*use_plus=*/false);
  replacements.SetQueryStr(query);
  return GURL("https://t1.gstatic.com/faviconV2")
      .ReplaceComponents(replacements);
}

affiliations::FacetBrandingInfo GetDefaultBrandingInfo(
    const CredentialUIEntry& credential) {
  return {GetShownOrigin(credential), GetIconUrl(credential.GetURL().spec())};
}

}  // namespace

class PasswordsGrouperTest : public ::testing::Test {
 protected:
  PasswordsGrouper& grouper() { return grouper_; }
  MockAffiliationService& affiliation_service() { return affiliation_service_; }

 private:
  MockAffiliationService affiliation_service_;
  PasswordsGrouper grouper_{&affiliation_service_};
};

TEST_F(PasswordsGrouperTest, GetAllCredentials) {
  StoredCredential form = CreateStoredCredential("https://test.com/");

  StoredCredential blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;

  StoredCredential federated_form;
  federated_form.url = GURL("https://test.com/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://test.com"));

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.com"))};
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<1>(std::vector<GroupedFacets>{
              std::move(group), GetSingleGroupForCredential(form)}));

  // These passkeys should be sorted by username and thus should be in the order
  // 3, 1, 2 in the output.
  PasskeyCredential passkey1 = CreatePasskey("test.com", "username1");
  PasskeyCredential passkey2 = CreatePasskey("test.com", "username2");
  PasskeyCredential passkey3 = CreatePasskey("test.com", "username0");

  CredentialUIEntry entry1(form);
  CredentialUIEntry entry_federated(federated_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form), std::move(blocked_form),
                            std::move(federated_form)),
      {passkey1, passkey2, passkey3}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAllCredentials(),
      ElementsAre(entry1, entry_federated, CredentialUIEntry(passkey3),
                  CredentialUIEntry(passkey1), CredentialUIEntry(passkey2)));
}

TEST_F(PasswordsGrouperTest, GetPasskeyFor) {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.com"))};
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{std::move(group)}));

  PasskeyCredential passkey = CreatePasskey("test.com");
  grouper().GroupCredentials(/*stored_credentials=*/{}, {passkey},
                             base::DoNothing());
  EXPECT_EQ(grouper().GetPasskeyFor(CredentialUIEntry(passkey)), passkey);
}

TEST_F(PasswordsGrouperTest, GetPasskeyForNoMatchingGroup) {
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{}));

  grouper().GroupCredentials(/*stored_credentials=*/{}, {}, base::DoNothing());
  PasskeyCredential passkey = CreatePasskey("notfound.com");
  EXPECT_FALSE(grouper().GetPasskeyFor(CredentialUIEntry(passkey)).has_value());
}

TEST_F(PasswordsGrouperTest, GetPasskeyNoPasskeyForMatchingGroup) {
  // Create a form for the same group so a form is found.
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.com"))};
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{std::move(group)}));
  grouper().GroupCredentials(
      MakeStoredCredentials(CreateStoredCredential("https://test.com/")), {},
      base::DoNothing());

  PasskeyCredential passkey = CreatePasskey("test.com");
  EXPECT_FALSE(grouper().GetPasskeyFor(CredentialUIEntry(passkey)).has_value());
}

TEST_F(PasswordsGrouperTest, GetAffiliatedGroupsWithGroupingInfo) {
  StoredCredential form = CreateStoredCredential("https://test.com/");

  StoredCredential blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;

  StoredCredential federated_form;
  federated_form.url = GURL("https://test.org/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federation.com"));

  std::vector<FacetURI> facets = {
      FacetURI::FromPotentiallyInvalidSpec(form.signon_realm),
      FacetURI::FromPotentiallyInvalidSpec(federated_form.url.spec())};

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.org"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo(facets, testing::_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<1>(std::vector<GroupedFacets>{
              group, GetSingleGroupForCredential(form)}));

  CredentialUIEntry credential1(form), credential2(federated_form),
      blocked_entry(blocked_form);
  StoredCredential form_copy = CloneStoredCredential(form);
  StoredCredential federated_copy = CloneStoredCredential(federated_form);
  StoredCredential blocked_copy = CloneStoredCredential(blocked_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form), std::move(federated_form),
                            std::move(blocked_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1}, GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential2}, GetDefaultBrandingInfo(credential2))));
  EXPECT_THAT(grouper().GetStoredCredentialsFor(credential1),
              ElementsAre(Eq(std::cref(form_copy))));
  EXPECT_THAT(grouper().GetStoredCredentialsFor(credential2),
              ElementsAre(Eq(std::cref(federated_copy))));

  EXPECT_THAT(grouper().GetBlockedSites(), ElementsAre(blocked_entry));
  EXPECT_THAT(grouper().GetStoredCredentialsFor(blocked_entry),
              ElementsAre(Eq(std::cref(blocked_copy))));
}

TEST_F(PasswordsGrouperTest, GroupPasswords) {
  StoredCredential form1 = CreateStoredCredential("https://test.com/");
  StoredCredential form2 = CreateStoredCredential(
      "https://affiliated-test.com/", u"username2", u"password2");

  StoredCredential blocked_form;
  blocked_form.signon_realm = "https://blocked.com/";
  blocked_form.url = GURL("https://blocked.com/");
  blocked_form.blocked_by_user = true;

  StoredCredential federated_form;
  federated_form.url = GURL("https://test.org/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federation.com"));

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form1.signon_realm)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(form2.signon_realm))};
  GroupedFacets federated_group;
  federated_group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(federated_form.url.spec()))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group, federated_group}));

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form), blocked_entry(blocked_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(blocked_form), std::move(federated_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential3}, GetDefaultBrandingInfo(credential3))));

  EXPECT_THAT(grouper().GetBlockedSites(), ElementsAre(blocked_entry));
}

TEST_F(PasswordsGrouperTest, GroupCredentialsWithoutAffiliation) {
  // Credentials saved for the same website should appear in the same group.
  StoredCredential form1 = CreateStoredCredential("https://test.com/");
  StoredCredential form2 =
      CreateStoredCredential("https://test.com/", u"username2", u"password2");

  StoredCredential blocked_form;
  blocked_form.signon_realm = "https://blocked.com/";
  blocked_form.url = GURL("https://blocked.com/");
  blocked_form.blocked_by_user = true;

  StoredCredential federated_form;
  federated_form.url = GURL("https://test.org/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federation.com"));

  GroupedFacets federated_group;
  federated_group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(federated_form.url.spec()))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<1>(std::vector<GroupedFacets>{
              federated_group, GetSingleGroupForCredential(form1)}));

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form), blocked_entry(blocked_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(blocked_form), std::move(federated_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential3}, GetDefaultBrandingInfo(credential3))));

  EXPECT_THAT(grouper().GetBlockedSites(), ElementsAre(blocked_entry));
}

TEST_F(PasswordsGrouperTest, HttpCredentialsSupported) {
  StoredCredential form = CreateStoredCredential("http://test.com/");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com/"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group}));

  CredentialUIEntry credential(form);
  StoredCredential form_copy = CloneStoredCredential(form);

  grouper().GroupCredentials(MakeStoredCredentials(std::move(form)),
                             /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup({credential},
                                          GetDefaultBrandingInfo(credential))));
  EXPECT_THAT(grouper().GetStoredCredentialsFor(credential),
              ElementsAre(Eq(std::cref(form_copy))));
}

TEST_F(PasswordsGrouperTest, FederatedCredentialsGroupedWithRegular) {
  StoredCredential form = CreateStoredCredential("https://test.com/");

  StoredCredential federated_form;
  federated_form.url = GURL("https://test.com/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federation.com"));

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{GetSingleGroupForCredential(form)}));

  CredentialUIEntry credential(form);
  CredentialUIEntry federated_credential(federated_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form), std::move(federated_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup({credential, federated_credential},
                                          GetDefaultBrandingInfo(credential))));
}

TEST_F(PasswordsGrouperTest, PasskeysGroupedWithPasswords) {
  StoredCredential form = CreateStoredCredential("https://test.com/");
  // These passkeys should be sorted by username and thus should be in the order
  // 3, 1, 2 in the output.
  PasskeyCredential passkey1 = CreatePasskey("test.com", "username1");
  PasskeyCredential passkey2 = CreatePasskey("test.com", "username2");
  PasskeyCredential passkey3 = CreatePasskey("test.com", "username0");

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{GetSingleGroupForCredential(form)}));

  CredentialUIEntry credential(form);

  grouper().GroupCredentials(MakeStoredCredentials(std::move(form)),
                             {passkey1, passkey2, passkey3}, base::DoNothing());

  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {credential, CredentialUIEntry(passkey3),
                   CredentialUIEntry(passkey1), CredentialUIEntry(passkey2)},
                  {GetDefaultBrandingInfo(credential)})));
}

TEST_F(PasswordsGrouperTest, GroupsWithMatchingMainDomainsMerged) {
  StoredCredential form1 = CreateStoredCredential("https://m.a.com/", u"test1");
  StoredCredential form2 = CreateStoredCredential("https://a.com/", u"test2");
  StoredCredential form3 = CreateStoredCredential("https://c.com/", u"test3");
  StoredCredential form4 = CreateStoredCredential("https://d.com/", u"test4");

  GroupedFacets group1;
  group1.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://a.com")),
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://c.com")),
  };
  group1.facets[0].main_domain = "a.com";
  group1.facets[1].main_domain = "c.com";

  GroupedFacets group2;
  group2.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://m.a.com"))};

  GroupedFacets group3;
  group3.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://d.com"))};
  std::vector<GroupedFacets> grouped_facets = {group1, group2, group3};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(grouped_facets));

  CredentialUIEntry credential1(form1), credential2(form2), credential3(form3),
      credential4(form4);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(form3), std::move(form4)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2, credential3},
                          GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential4}, GetDefaultBrandingInfo(credential4))));
}

TEST_F(PasswordsGrouperTest, MainDomainComputationUsesPSLExtensions) {
  StoredCredential form1 = CreateStoredCredential("https://m.a.com/", u"test1");
  StoredCredential form2 = CreateStoredCredential("https://b.a.com/", u"test2");
  StoredCredential form3 =
      CreateStoredCredential("https://c.b.a.com/", u"test3");
  StoredCredential form4 = CreateStoredCredential("https://a.com/", u"test4");

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          std::vector<std::string>{"a.com"}));
  PasswordsGrouper grouper(&affiliation_service());

  // Create an individual group for each form.
  std::vector<GroupedFacets> grouped_facets;
  for (const auto& realm : {"https://m.a.com/", "https://b.a.com/",
                            "https://c.b.a.com/", "https://a.com/"}) {
    GroupedFacets group;
    group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(realm));
    grouped_facets.push_back(std::move(group));
  }
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(grouped_facets));

  CredentialUIEntry credential1(form1), credential2(form2), credential3(form3),
      credential4(form4);

  grouper.GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(form3), std::move(form4)),
      /*passkeys=*/{}, base::DoNothing());

  // a.com is considered eTLD+1 but since a.com is present in PSL Extension List
  // main domains for |forms| would be m.a.com, b.a.com, b.a.com and a.com, thus
  // only forms for b.a.com are grouped.
  EXPECT_THAT(
      grouper.GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1},
                          {"m.a.com", GetIconUrl(credential1.GetURL().spec())}),
          AffiliatedGroup({credential2, credential3},
                          {"b.a.com", GetIconUrl(credential2.GetURL().spec())}),
          AffiliatedGroup({credential4},
                          {"a.com", GetIconUrl(credential4.GetURL().spec())})));
}

TEST_F(PasswordsGrouperTest, HttpAndHttpsGroupedTogether) {
  StoredCredential form1 = CreateStoredCredential("http://test.com/");
  StoredCredential form2 = CreateStoredCredential("https://test.com/");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com/"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<1>(std::vector<GroupedFacets>{
              group, GetSingleGroupForCredential(form2)}));

  CredentialUIEntry credential(MakeStoredCredentials(
      CloneStoredCredential(form1), CloneStoredCredential(form2)));
  StoredCredential form1_copy = CloneStoredCredential(form1);
  StoredCredential form2_copy = CloneStoredCredential(form2);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup({credential},
                                          GetDefaultBrandingInfo(credential))));
  EXPECT_THAT(grouper().GetStoredCredentialsFor(credential),
              UnorderedElementsAre(Eq(std::cref(form1_copy)),
                                   Eq(std::cref(form2_copy))));
}

TEST_F(PasswordsGrouperTest, FederatedAndroidAppGroupedWithRegularPasswords) {
  StoredCredential form = CreateStoredCredential("https://test.app.com/");
  StoredCredential federated_android_form;
  federated_android_form.signon_realm =
      "android://"
      "5Z0D_o6B8BqileZyWhXmqO_wkO8uO0etCEXvMn5tUzEqkWUgfTSjMcTM7eMMTY_"
      "FGJC9RlpRNt_8Qp5tgDocXw==@com.bambuna.podcastaddict/";
  federated_android_form.username_value = u"test@gmail.com";
  federated_android_form.url = GURL(federated_android_form.signon_realm);
  federated_android_form.federation_origin =
      url::SchemeHostPort(GURL(u"https://federatedOrigin.com"));

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(
          "android://"
          "5Z0D_o6B8BqileZyWhXmqO_wkO8uO0etCEXvMn5tUzEqkWUgfTSjMcTM7eMMTY_"
          "FGJC9RlpRNt_8Qp5tgDocXw==@com.bambuna.podcastaddict")),
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.app.com")),
  };

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group}));

  CredentialUIEntry credential(form),
      federated_credential(federated_android_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form), std::move(federated_android_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {federated_credential, credential},
                  {GetShownOrigin(federated_credential),
                   GURL("https://www.gstatic.com/images/branding/product/1x/"
                        "play_apps_32dp.png")})));
}

TEST_F(PasswordsGrouperTest, EncodedCharactersInSignonRealm) {
  StoredCredential form =
      CreateStoredCredential("https://test.com/sign in/%-.<>`^_'{|}");

  // For federated credentials url is used for grouping. Add space there.
  StoredCredential federated_form;
  federated_form.url = GURL("https://test.org/sign in/%-.<>`^_'{|}");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federation.com"));

  GroupedFacets group;
  // Group them only by TLD.
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec("https://test.com")),
      Facet(FacetURI::FromCanonicalSpec("https://test.org")),
  };

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group}));

  CredentialUIEntry credential1(form), credential2(federated_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form), std::move(federated_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(AffiliatedGroup(
          {credential1, credential2}, GetDefaultBrandingInfo(credential1))));
}

TEST_F(PasswordsGrouperTest, OrderIsCaseInsensitive) {
  StoredCredential form1 = CreateStoredCredential("https://test1.com");
  StoredCredential form2 = CreateStoredCredential("https://test2.com");
  StoredCredential form3 = CreateStoredCredential("https://test3.com");

  GroupedFacets group1 = GetSingleGroupForCredential(form1);
  group1.branding_info.name = "beta";
  group1.branding_info.icon_url = GURL("https://test.com/favicon.ico");

  GroupedFacets group2 = GetSingleGroupForCredential(form2);
  group2.branding_info.name = "Gamma";
  group2.branding_info.icon_url = GURL("https://test.com/favicon.ico");

  GroupedFacets group3 = GetSingleGroupForCredential(form3);
  group3.branding_info.name = "Alpha";
  group3.branding_info.icon_url = GURL("https://test.com/favicon.ico");

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group1, group2, group3}));

  CredentialUIEntry credential1(form1), credential2(form2), credential3(form3);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(form3)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(AffiliatedGroup({credential3}, group3.branding_info),
                  AffiliatedGroup({credential1}, group1.branding_info),
                  AffiliatedGroup({credential2}, group2.branding_info)));
}

TEST_F(PasswordsGrouperTest, IpAddressesGroupedTogether) {
  StoredCredential form1 =
      CreateStoredCredential("https://192.168.1.1/tomato", u"admin");
  StoredCredential form2 = CreateStoredCredential(
      "https://192.168.1.1/TP-LINK Wireless AP WA501G", u"admin");
  StoredCredential form3 =
      CreateStoredCredential("https://192.168.1.1/", u"linkhub");
  StoredCredential form4 =
      CreateStoredCredential("https://192.168.1.1/", u"root");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec("https://192.168.1.1")),
  };

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group}));

  std::string icon_url_realm = form1.signon_realm;
  CredentialUIEntry credential1(MakeStoredCredentials(
      CloneStoredCredential(form1), CloneStoredCredential(form2))),
      credential2(form3), credential3(form4);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(form3), std::move(form4)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              UnorderedElementsAre(AffiliatedGroup(
                  {credential1, credential2, credential3},
                  {"https://192.168.1.1", GetIconUrl(icon_url_realm)})));
}

TEST_F(PasswordsGrouperTest, SchemeOmittedDuringOrdering) {
  StoredCredential form1 = CreateStoredCredential("https://a.com");
  StoredCredential form2 = CreateStoredCredential("https://b.com");
  StoredCredential ip_form = CreateStoredCredential("https://192.168.1.1/");

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{GetSingleGroupForCredential(form1),
                                     GetSingleGroupForCredential(form2),
                                     GetSingleGroupForCredential(ip_form)}));

  std::string ip_realm = ip_form.signon_realm;
  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(ip_form);

  grouper().GroupCredentials(
      MakeStoredCredentials(std::move(form1), std::move(form2),
                            std::move(ip_form)),
      /*passkeys=*/{}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(
          AffiliatedGroup({credential3},
                          {"https://192.168.1.1", GetIconUrl(ip_realm)}),
          AffiliatedGroup({credential1}, GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential2}, GetDefaultBrandingInfo(credential2))));
}

TEST_F(PasswordsGrouperTest, BlockedSitesOmitDuplicates) {
  StoredCredential blocked_form_1;
  blocked_form_1.signon_realm = "https://test.com/";
  blocked_form_1.url = GURL(blocked_form_1.signon_realm);
  blocked_form_1.blocked_by_user = true;

  StoredCredential blocked_form_2;
  blocked_form_2.signon_realm = "https://test.com/auth";
  blocked_form_2.url = GURL(blocked_form_2.signon_realm);
  blocked_form_2.blocked_by_user = true;

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{}));

  CredentialUIEntry blocked_entry(blocked_form_1);

  grouper().GroupCredentials(MakeStoredCredentials(std::move(blocked_form_1),
                                                   std::move(blocked_form_2)),
                             {}, base::DoNothing());

  EXPECT_THAT(grouper().GetBlockedSites(), ElementsAre(blocked_entry));
}

}  // namespace password_manager
