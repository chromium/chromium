// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

namespace password_manager {

namespace {

PasswordForm CreateForm(std::string sinon_realm,
                        std::u16string username = u"username",
                        std::u16string password = u"password") {
  PasswordForm form;
  form.signon_realm = sinon_realm;
  form.username_value = username;
  form.password_value = password;
  return form;
}

GroupedFacets GetSingleGroupForForm(PasswordForm form) {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm))};
  return group;
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
      url::Origin::Create(GURL("https://accounts.federation.com"));

  std::vector<FacetURI> facets = {
      FacetURI::FromPotentiallyInvalidSpec(form.signon_realm),
      FacetURI::FromPotentiallyInvalidSpec(federated_form.url.spec())};

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.org"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo(facets, testing::_))
      .WillRepeatedly(base::test::RunOnceCallback<1>(
          std::vector<GroupedFacets>{group, GetSingleGroupForForm(form)}));
  grouper().GroupPasswords({form, federated_form, blocked_form},
                           base::DoNothing());

  CredentialUIEntry credential1(form), credential2(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1}, {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential2}, {GetShownOrigin(credential2)})));
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
      url::Origin::Create(GURL("https://accounts.federation.com"));

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form1.signon_realm)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(form2.signon_realm))};
  GroupedFacets federated_group;
  federated_group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(federated_form.url.spec()))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallback<1>(
          std::vector<GroupedFacets>{group, federated_group}));
  grouper().GroupPasswords({form1, form2, blocked_form, federated_form},
                           base::DoNothing());

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential3}, {GetShownOrigin(credential3)})));

  EXPECT_THAT(grouper().GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST_F(PasswordsGrouperTest, GroupPasswordsWithoutAffiliation) {
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
      url::Origin::Create(GURL("https://accounts.federation.com"));

  GroupedFacets federated_group;
  federated_group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(federated_form.url.spec()))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallback<1>(std::vector<GroupedFacets>{
          federated_group, GetSingleGroupForForm(form1)}));
  grouper().GroupPasswords({form1, form2, blocked_form, federated_form},
                           base::DoNothing());

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential3}, {GetShownOrigin(credential3)})));

  EXPECT_THAT(grouper().GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST_F(PasswordsGrouperTest, HttpCredentialsSupported) {
  PasswordForm form = CreateForm("http://test.com/");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com/"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallback<1>(std::vector<GroupedFacets>{group}));
  grouper().GroupPasswords({form}, base::DoNothing());

  CredentialUIEntry credential(form);
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(AffiliatedGroup({credential}, {GetShownOrigin(credential)})));
  EXPECT_THAT(grouper().GetPasswordFormsFor(credential), ElementsAre(form));
}

TEST_F(PasswordsGrouperTest, FederatedCredentialsGroupedWithRegular) {
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm federated_form;
  federated_form.url = GURL("https://test.com/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://accounts.federation.com"));

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallback<1>(
          std::vector<GroupedFacets>{GetSingleGroupForForm(form)}));
  grouper().GroupPasswords({form, federated_form}, base::DoNothing());

  CredentialUIEntry credential(form);
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {credential, CredentialUIEntry(federated_form)},
                  {GetShownOrigin(credential)})));
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
      .WillRepeatedly(base::test::RunOnceCallback<1>(grouped_facets));
  grouper().GroupPasswords(forms, base::DoNothing());

  CredentialUIEntry credential1(forms[0]), credential2(forms[1]),
      credential3(forms[2]), credential4(forms[3]);

  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2, credential3},
                          {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential4}, {GetShownOrigin(credential4)})));
}

TEST_F(PasswordsGrouperTest, MainDomainComputationUsesPSLExtensions) {
  std::vector<PasswordForm> forms = {CreateForm("https://m.a.com/", u"test1"),
                                     CreateForm("https://b.a.com/", u"test2"),
                                     CreateForm("https://c.b.a.com/", u"test3"),
                                     CreateForm("https://a.com/", u"test4")};

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillRepeatedly(
          base::test::RunOnceCallback<0>(std::vector<std::string>{"a.com"}));
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
      .WillRepeatedly(base::test::RunOnceCallback<1>(grouped_facets));

  grouper.GroupPasswords(forms, base::DoNothing());

  CredentialUIEntry credential1(forms[0]), credential2(forms[1]),
      credential3(forms[2]), credential4(forms[3]);

  // a.com is considered eTLD+1 but since a.com is present in PSL Extension List
  // main domains for |forms| would be m.a.com, b.a.com, b.a.com and a.com, thus
  // only forms for b.a.com are grouped.
  EXPECT_THAT(
      grouper.GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1}, {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential2, credential3},
                          {GetShownOrigin(credential2)}),
          AffiliatedGroup({credential4}, {GetShownOrigin(credential4)})));
}

TEST_F(PasswordsGrouperTest, HttpAndHttpsGroupedTogether) {
  PasswordForm form1 = CreateForm("http://test.com/");
  PasswordForm form2 = CreateForm("https://test.com/");

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com/"))};

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallback<1>(
          std::vector<GroupedFacets>{group, GetSingleGroupForForm(form2)}));
  grouper().GroupPasswords({form1, form2}, base::DoNothing());

  CredentialUIEntry credential({form1, form2});
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(AffiliatedGroup({credential}, {GetShownOrigin(credential)})));
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
      url::Origin::Create(GURL(u"https://federatedOrigin.com"));

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(
          "android://"
          "5Z0D_o6B8BqileZyWhXmqO_wkO8uO0etCEXvMn5tUzEqkWUgfTSjMcTM7eMMTY_"
          "FGJC9RlpRNt_8Qp5tgDocXw==@com.bambuna.podcastaddict")),
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://test.app.com")),
  };

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallback<1>(std::vector<GroupedFacets>{group}));
  grouper().GroupPasswords({form, federated_android_form}, base::DoNothing());

  CredentialUIEntry credential({form}),
      federated_credential({federated_android_form});
  EXPECT_THAT(
      grouper().GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(AffiliatedGroup({federated_credential, credential},
                                  {GetShownOrigin(federated_credential)})));
}

TEST_F(PasswordsGrouperTest, EncodedCharactersInSignonRealm) {
  PasswordForm form = CreateForm("https://test.com/sign in/%-.<>`^_'{|}");

  // For federated credentials url is used for grouping. Add space there.
  PasswordForm federated_form;
  federated_form.url = GURL("https://test.org/sign in/%-.<>`^_'{|}");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://accounts.federation.com"));

  GroupedFacets group;
  // Group them only by TLD.
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(
          "https://test.com/sign%20in/%-.%3C%3E%60%5E_'%7B%7C%7D")),
      Facet(FacetURI::FromCanonicalSpec("https://test.org")),
  };

  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(
          base::test::RunOnceCallback<1>(std::vector<GroupedFacets>{group}));
  grouper().GroupPasswords({form, federated_form}, base::DoNothing());

  CredentialUIEntry credential1(form), credential2(federated_form);
  EXPECT_THAT(grouper().GetAffiliatedGroupsWithGroupingInfo(),
              UnorderedElementsAre(AffiliatedGroup(
                  {credential1, credential2}, {GetShownOrigin(credential1)})));
}

}  // namespace password_manager
