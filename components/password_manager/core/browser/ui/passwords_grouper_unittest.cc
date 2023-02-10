// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include <utility>
#include <vector>

#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
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

}  // namespace

TEST(PasswordsGrouperTest, GetAffiliatedGroupsWithGroupingInfo) {
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
  federated_form.signon_realm = "https://federated.com/";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::Origin::Create(GURL(u"federatedOrigin.com"));

  PasswordsGrouper grouper;
  grouper.GroupPasswords(
      {}, {std::make_pair("key1", form), std::make_pair("key2", federated_form),
           std::make_pair("key3", blocked_form)});

  CredentialUIEntry credential1(form), credential2(federated_form);
  EXPECT_THAT(
      grouper.GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1}, {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential2}, {GetShownOrigin(credential2)})));

  EXPECT_THAT(grouper.GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST(PasswordsGrouperTest, GroupPasswords) {
  PasswordForm form1 = CreateForm("https://test.com/");
  PasswordForm form2 =
      CreateForm("https://affiliated-test.com/", u"username2", u"password2");

  PasswordForm blocked_form;
  blocked_form.signon_realm = blocked_form.url.spec();
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
  federated_form.signon_realm = "https://federated.com/";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::Origin::Create(GURL(u"federatedOrigin.com"));

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form1.signon_realm)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(form2.signon_realm))};

  PasswordsGrouper grouper;
  grouper.GroupPasswords(
      {group}, {std::make_pair("key1", form1), std::make_pair("key2", form2),
                std::make_pair("key3", blocked_form),
                std::make_pair("key4", federated_form)});

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form);
  EXPECT_THAT(
      grouper.GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential3}, {GetShownOrigin(credential3)})));

  EXPECT_THAT(grouper.GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST(PasswordsGrouperTest, GroupPasswordsWithoutAffiliation) {
  // Credentials saved for the same website should appear in the same group.
  PasswordForm form1 = CreateForm("https://test.com/");
  PasswordForm form2 =
      CreateForm("https://test.com/", u"username2", u"password2");

  PasswordForm blocked_form;
  blocked_form.signon_realm = blocked_form.url.spec();
  blocked_form.blocked_by_user = true;

  PasswordForm federated_form;
  federated_form.signon_realm = "https://federated.com/";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::Origin::Create(GURL(u"federatedOrigin.com"));

  PasswordsGrouper grouper;
  grouper.GroupPasswords(
      {}, {std::make_pair("key1", form1), std::make_pair("key2", form2),
           std::make_pair("key3", blocked_form),
           std::make_pair("key4", federated_form)});

  CredentialUIEntry credential1(form1), credential2(form2),
      credential3(federated_form);
  EXPECT_THAT(
      grouper.GetAffiliatedGroupsWithGroupingInfo(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          {GetShownOrigin(credential1)}),
          AffiliatedGroup({credential3}, {GetShownOrigin(credential3)})));

  EXPECT_THAT(grouper.GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));
}

TEST(PasswordsGrouperTest, HttpCredentialsSupported) {
  PasswordForm form = CreateForm("http://test.com/");

  PasswordsGrouper grouper;
  grouper.GroupPasswords({}, {std::make_pair("key1", form)});

  CredentialUIEntry credential(form);
  EXPECT_THAT(
      grouper.GetAffiliatedGroupsWithGroupingInfo(),
      ElementsAre(AffiliatedGroup({credential}, {GetShownOrigin(credential)})));
}

TEST(PasswordsGrouperTest, FederatedCredentialsGroupedWithRegular) {
  PasswordForm form = CreateForm("https://test.com/");

  PasswordForm federated_form;
  federated_form.url = GURL("https://test.com/");
  federated_form.signon_realm = "federation://test.com/accounts.federation.com";
  federated_form.username_value = u"username2";
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://accounts.federation.com"));

  PasswordsGrouper grouper;
  grouper.GroupPasswords({}, {std::make_pair("key1", form),
                              std::make_pair("key2", federated_form)});

  CredentialUIEntry credential(form);
  EXPECT_THAT(grouper.GetAffiliatedGroupsWithGroupingInfo(),
              ElementsAre(AffiliatedGroup(
                  {credential, CredentialUIEntry(federated_form)},
                  {GetShownOrigin(credential)})));
}

}  // namespace password_manager
