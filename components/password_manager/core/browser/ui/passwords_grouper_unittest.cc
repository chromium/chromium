// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/test/gmock_callback_support.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
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

PasswordForm CreateForm(std::string signon_realm,
                        std::u16string username = u"username",
                        std::u16string password = u"password") {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.username_value = username;
  form.password_value = password;
  form.url = GURL(signon_realm);
  return form;
}

GroupedFacets GetSingleGroupForForm(PasswordForm form) {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm))};
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
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
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
              std::move(group), GetSingleGroupForForm(form)}));

  // These passkeys should be sorted by username and thus should be in the order
  // 3, 1, 2 in the output.
  PasskeyCredential passkey1 = CreatePasskey("test.com", "username1");
  PasskeyCredential passkey2 = CreatePasskey("test.com", "username2");
  PasskeyCredential passkey3 = CreatePasskey("test.com", "username0");
  grouper().GroupCredentials({form, blocked_form, federated_form},
                             {passkey1, passkey2, passkey3}, base::DoNothing());

  EXPECT_THAT(
      grouper().GetAllCredentials(),
      ElementsAre(CredentialUIEntry(form), CredentialUIEntry(federated_form),
                  CredentialUIEntry(passkey3), CredentialUIEntry(passkey1),
                  CredentialUIEntry(passkey2)));
}

TEST_F(PasswordsGrouperTest, GetPasskeyFor) {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.com"))};
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{std::move(group)}));

  PasskeyCredential passkey = CreatePasskey("test.com");
  grouper().GroupCredentials(/*password_forms=*/{}, {passkey},
                             base::DoNothing());
  EXPECT_EQ(grouper().GetPasskeyFor(CredentialUIEntry(passkey)), passkey);
}

TEST_F(PasswordsGrouperTest, GetPasskeyForNoMatchingGroup) {
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{}));

  grouper().GroupCredentials(/*password_forms=*/{}, {}, base::DoNothing());
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
  grouper().GroupCredentials({CreateForm("https://test.com/")}, {},
                             base::DoNothing());

  PasskeyCredential passkey = CreatePasskey("test.com");
  EXPECT_FALSE(grouper().GetPasskeyFor(CredentialUIEntry(passkey)).has_value());
}

TEST_F(PasswordsGrouperTest, GetAffiliatedGroupsWithGroupingInfo) {
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
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
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group, GetSingleGroupForForm(form)}));
  grouper().GroupCredentials({form, federated_form, blocked_form},
                             /*passkeys=*/{}, base::DoNothing());

  CredentialUIEntry credential1(form), credential2(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1}, GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential2}, GetDefaultBrandingInfo(credential2))));
  EXPECT_THAT(grouper().GetPasswordFormsFor(credential1), ElementsAre(form));
  EXPECT_THAT(grouper().GetPasswordFormsFor(credential2),
              ElementsAre(federated_form));

  EXPECT_THAT(grouper().GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
  EXPECT_THAT(grouper().GetPasswordFormsFor(CredentialUIEntry(blocked_form)),
              ElementsAre(blocked_form));
}

TEST_F(PasswordsGrouperTest, GroupPasswords) {
  PasswordForm form1 = CreateForm("https://test.com/");
  PasswordForm form2 =
      CreateForm("https://affiliated-test.com/", u"username2", u"password2");

  PasswordForm blocked_form;
  blocked_form.signon_realm = blocked_form.url.spec();
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
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
  grouper().GroupCredentials({form1, form2, blocked_form, federated_form},
                             /*passkeys=*/{}, base::DoNothing());

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential3}, GetDefaultBrandingInfo(credential3))));

  EXPECT_THAT(grouper().GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST_F(PasswordsGrouperTest, GroupCredentialsWithoutAffiliation) {
  // Credentials saved for the same website should appear in the same group.
  PasswordForm form1 = CreateForm("https://test.com/");
  PasswordForm form2 =
      CreateForm("https://test.com/", u"username2", u"password2");

  PasswordForm blocked_form;
  blocked_form.signon_realm = blocked_form.url.spec();
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
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
              federated_group, GetSingleGroupForForm(form1)}));
  grouper().GroupCredentials({form1, form2, blocked_form, federated_form},
                             /*passkeys=*/{}, base::DoNothing());

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential3}, GetDefaultBrandingInfo(credential3))));

  EXPECT_THAT(grouper().GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST_F(PasswordsGrouperTest, HttpCredentialsSupported) {
  PasswordForm form = CreateForm("http://test.com/");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com/"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group}));
  grouper().GroupCredentials({form}, /*passkeys=*/{}, base::DoNothing());

  CredentialUIEntry credential(form);
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup({credential},
                                          GetDefaultBrandingInfo(credential))));
  EXPECT_THAT(grouper().GetPasswordFormsFor(credential), ElementsAre(form));
}

TEST_F(PasswordsGrouperTest, FederatedCredentialsGroupedWithRegular) {
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm federated_form;
  federated_form.url = GURL("https://test.com/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("https://accounts.federation.com"));

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{GetSingleGroupForForm(form)}));
  grouper().GroupCredentials({form, federated_form}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential(form);
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {credential, CredentialUIEntry(federated_form)},
                  GetDefaultBrandingInfo(credential))));
}

TEST_F(PasswordsGrouperTest, PasskeysGroupedWithPasswords) {
  PasswordForm form = CreateForm("https://test.com/");
  // These passkeys should be sorted by username and thus should be in the order
  // 3, 1, 2 in the output.
  PasskeyCredential passkey1 = CreatePasskey("test.com", "username1");
  PasskeyCredential passkey2 = CreatePasskey("test.com", "username2");
  PasskeyCredential passkey3 = CreatePasskey("test.com", "username0");

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{GetSingleGroupForForm(form)}));
  grouper().GroupCredentials({form}, {passkey1, passkey2, passkey3},
                             base::DoNothing());

  CredentialUIEntry credential(form);
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {credential, CredentialUIEntry(passkey3),
                   CredentialUIEntry(passkey1), CredentialUIEntry(passkey2)},
                  {GetDefaultBrandingInfo(credential)})));
}

TEST_F(PasswordsGrouperTest, GroupsWithMatchingMainDomainsMerged) {
  std::vector<PasswordForm> forms = {CreateForm("https://m.a.com/", u"test1"),
                                     CreateForm("https://a.com/", u"test2"),
                                     CreateForm("https://c.com/", u"test3"),
                                     CreateForm("https://d.com/", u"test4")};

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
  std::vector<password_manager::GroupedFacets> grouped_facets = {group1, group2,
                                                                 group3};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(grouped_facets));
  grouper().GroupCredentials(forms, /*passkeys=*/{}, base::DoNothing());

  CredentialUIEntry credential1(forms[0]), credential2(forms[1]),
      credential3(forms[2]), credential4(forms[3]);

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2, credential3},
                          GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential4}, GetDefaultBrandingInfo(credential4))));
}

TEST_F(PasswordsGrouperTest, MainDomainComputationUsesPSLExtensions) {
  std::vector<PasswordForm> forms = {CreateForm("https://m.a.com/", u"test1"),
                                     CreateForm("https://b.a.com/", u"test2"),
                                     CreateForm("https://c.b.a.com/", u"test3"),
                                     CreateForm("https://a.com/", u"test4")};

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          std::vector<std::string>{"a.com"}));
  PasswordsGrouper grouper(&affiliation_service());

  // Create an individual group for each form.
  std::vector<password_manager::GroupedFacets> grouped_facets;
  for (const auto& form : forms) {
    GroupedFacets group;
    group.facets.emplace_back(
        FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
    grouped_facets.push_back(std::move(group));
  }
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(grouped_facets));

  grouper.GroupCredentials(forms, /*passkeys=*/{}, base::DoNothing());

  CredentialUIEntry credential1(forms[0]), credential2(forms[1]),
      credential3(forms[2]), credential4(forms[3]);

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
  PasswordForm form1 = CreateForm("http://test.com/");
  PasswordForm form2 = CreateForm("https://test.com/");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com/"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group, GetSingleGroupForForm(form2)}));
  grouper().GroupCredentials({form1, form2}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential({form1, form2});
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup({credential},
                                          GetDefaultBrandingInfo(credential))));
  EXPECT_THAT(grouper().GetPasswordFormsFor(credential),
              UnorderedElementsAre(form1, form2));
}

TEST_F(PasswordsGrouperTest, FederatedAndroidAppGroupedWithRegularPasswords) {
  PasswordForm form = CreateForm("https://test.app.com/");
  PasswordForm federated_android_form;
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
  grouper().GroupCredentials({form, federated_android_form}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential({form}),
      federated_credential({federated_android_form});
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {federated_credential, credential},
                  {GetShownOrigin(federated_credential),
                   GURL("https://www.gstatic.com/images/branding/product/1x/"
                        "play_apps_32dp.png")})));
}

TEST_F(PasswordsGrouperTest, EncodedCharactersInSignonRealm) {
  PasswordForm form = CreateForm("https://test.com/sign in/%-.<>`^_'{|}");

  // For federated credentials url is used for grouping. Add space there.
  PasswordForm federated_form;
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
  grouper().GroupCredentials({form, federated_form}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential1(form), credential2(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(AffiliatedGroup(
          {credential1, credential2}, GetDefaultBrandingInfo(credential1))));
}

TEST_F(PasswordsGrouperTest, OrderIsCaseInsensitive) {
  PasswordForm form1 = CreateForm("https://test1.com");
  PasswordForm form2 = CreateForm("https://test2.com");
  PasswordForm form3 = CreateForm("https://test3.com");

  GroupedFacets group1 = GetSingleGroupForForm(form1);
  group1.branding_info.name = "beta";
  group1.branding_info.icon_url = GURL("https://test.com/favicon.ico");

  GroupedFacets group2 = GetSingleGroupForForm(form2);
  group2.branding_info.name = "Gamma";
  group2.branding_info.icon_url = GURL("https://test.com/favicon.ico");

  GroupedFacets group3 = GetSingleGroupForForm(form3);
  group3.branding_info.name = "Alpha";
  group3.branding_info.icon_url = GURL("https://test.com/favicon.ico");

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group1, group2, group3}));
  grouper().GroupCredentials({form1, form2, form3}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential1(form1), credential2(form2), credential3(form3);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(AffiliatedGroup({credential3}, group3.branding_info),
                  AffiliatedGroup({credential1}, group1.branding_info),
                  AffiliatedGroup({credential2}, group2.branding_info)));
}

TEST_F(PasswordsGrouperTest, IpAddressesGroupedTogether) {
  PasswordForm form1 = CreateForm("https://192.168.1.1/tomato", u"admin");
  PasswordForm form2 =
      CreateForm("https://192.168.1.1/TP-LINK Wireless AP WA501G", u"admin");
  PasswordForm form3 = CreateForm("https://192.168.1.1/", u"linkhub");
  PasswordForm form4 = CreateForm("https://192.168.1.1/", u"root");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec("https://192.168.1.1")),
  };

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{group}));
  grouper().GroupCredentials({form1, form2, form3, form4}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential1({form1, form2}), credential2(form3),
      credential3(form4);
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              UnorderedElementsAre(AffiliatedGroup(
                  {credential1, credential2, credential3},
                  {"https://192.168.1.1", GetIconUrl(form1.signon_realm)})));
}

TEST_F(PasswordsGrouperTest, SchemeOmittedDuringOrdering) {
  PasswordForm form1 = CreateForm("https://a.com");
  PasswordForm form2 = CreateForm("https://b.com");
  PasswordForm ip_form = CreateForm("https://192.168.1.1/");

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<1>(std::vector<GroupedFacets>{
              GetSingleGroupForForm(form1), GetSingleGroupForForm(form2),
              GetSingleGroupForForm(ip_form)}));
  grouper().GroupCredentials({form1, form2, ip_form}, /*passkeys=*/{},
                             base::DoNothing());

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(ip_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(
          AffiliatedGroup({credential3}, {"https://192.168.1.1",
                                          GetIconUrl(ip_form.signon_realm)}),
          AffiliatedGroup({credential1}, GetDefaultBrandingInfo(credential1)),
          AffiliatedGroup({credential2}, GetDefaultBrandingInfo(credential2))));
}

TEST_F(PasswordsGrouperTest, BlockedSitesOmitDuplicates) {
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm blocked_form_1;
  blocked_form_1.signon_realm = "https://test.com/";
  blocked_form_1.url = GURL(blocked_form_1.signon_realm);
  blocked_form_1.blocked_by_user = true;

  PasswordForm blocked_form_2;
  blocked_form_2.signon_realm = "https://test.com/auth";
  blocked_form_2.url = GURL(blocked_form_2.signon_realm);
  blocked_form_2.blocked_by_user = true;

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<GroupedFacets>{}));
  grouper().GroupCredentials({blocked_form_1, blocked_form_2}, {},
                             base::DoNothing());

  EXPECT_THAT(grouper().GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form_1)));
}

}  // namespace password_manager
