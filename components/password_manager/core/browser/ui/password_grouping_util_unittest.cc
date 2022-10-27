// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_grouping_util.h"

#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::UnorderedElementsAre;

namespace password_manager {

TEST(PasswordGroupingUtilTest, GetAffiliatedGroupsWithGroupingInfo) {
  PasswordForm form;
  form.url = GURL("https://test.com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm federated_form;
  federated_form.signon_realm = "https://federated.com";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::Origin::Create(GURL(u"federatedOrigin.com"));
  federated_form.in_store = PasswordForm::Store::kProfileStore;

  std::map<GroupId, std::map<UsernamePasswordKey, std::vector<PasswordForm>>>
      map_group_id_to_forms;
  GroupId group_id1(1);
  UsernamePasswordKey test_key1("1234");
  map_group_id_to_forms[group_id1][test_key1].push_back(form);
  UsernamePasswordKey test_key2("5678");
  map_group_id_to_forms[group_id1][test_key2].push_back(blocked_form);
  GroupId group_id2(2);
  UsernamePasswordKey test_key3("aaaa");
  map_group_id_to_forms[group_id2][test_key3].push_back(federated_form);

  PasswordGroupingInfo password_grouping_info;
  password_grouping_info.map_group_id_to_forms = map_group_id_to_forms;

  // Setup results to compare.
  std::vector<CredentialUIEntry> credential_group1;
  credential_group1.emplace_back(form);
  credential_group1.emplace_back(blocked_form);
  std::vector<CredentialUIEntry> credential_group2;
  credential_group2.emplace_back(federated_form);

  EXPECT_THAT(GetAffiliatedGroupsWithGroupingInfo(password_grouping_info),
              UnorderedElementsAre(AffiliatedGroup(credential_group1),
                                   AffiliatedGroup(credential_group2)));
}

TEST(PasswordGroupingUtilTest, GroupPasswords) {
  PasswordForm form;
  form.url = GURL("https://test.com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm form2;
  form2.url = form.url;
  form2.signon_realm = form.signon_realm;
  form2.username_value = u"username2";
  form2.password_value = u"password2";
  form2.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm federated_form;
  federated_form.signon_realm = "https://federated.com";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::Origin::Create(GURL(u"federatedOrigin.com"));
  federated_form.in_store = PasswordForm::Store::kProfileStore;

  // Create grouped facets vector for test.
  std::vector<password_manager::GroupedFacets> grouped_facets_vect;

  // Form & Blocked form.
  Facet facet;
  facet.uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  GroupedFacets grouped_facets;
  grouped_facets.facets.push_back(std::move(facet));
  grouped_facets_vect.push_back(std::move(grouped_facets));

  // Federated form.
  Facet facet2;
  facet2.uri =
      FacetURI::FromPotentiallyInvalidSpec(federated_form.signon_realm);
  GroupedFacets grouped_facets2;
  grouped_facets2.facets.push_back(std::move(facet2));
  grouped_facets_vect.push_back(std::move(grouped_facets2));

  // Create sort_key_to_password_forms object for test.
  std::multimap<std::string, PasswordForm> sort_key_to_password_forms;
  sort_key_to_password_forms.insert(std::make_pair("test_key1", form));
  sort_key_to_password_forms.insert(std::make_pair("test_key2", form2));
  sort_key_to_password_forms.insert(std::make_pair("test_key3", blocked_form));
  sort_key_to_password_forms.insert(
      std::make_pair("test_key4", federated_form));

  // Create map_group_id_to_forms object for test.
  std::map<GroupId, std::map<UsernamePasswordKey, std::vector<PasswordForm>>>
      map_group_id_to_forms;
  GroupId group_id1(1);
  UsernamePasswordKey test_key1(CreateUsernamePasswordSortKey(form));
  map_group_id_to_forms[group_id1][test_key1].push_back(form);
  UsernamePasswordKey test_key2(CreateUsernamePasswordSortKey(form2));
  map_group_id_to_forms[group_id1][test_key2].push_back(form2);
  GroupId group_id2(2);
  UsernamePasswordKey test_key3(CreateUsernamePasswordSortKey(federated_form));
  map_group_id_to_forms[group_id2][test_key3].push_back(federated_form);

  PasswordGroupingInfo expected_password_grouping_info;
  expected_password_grouping_info.map_group_id_to_forms = map_group_id_to_forms;

  std::vector<password_manager::CredentialUIEntry> expected_blocked_sites;
  expected_blocked_sites.emplace_back(blocked_form);

  PasswordGroupingInfo password_grouping_info =
      GroupPasswords(grouped_facets_vect, sort_key_to_password_forms);
  EXPECT_THAT(password_grouping_info.map_group_id_to_forms,
              expected_password_grouping_info.map_group_id_to_forms);
  EXPECT_THAT(password_grouping_info.blocked_sites, expected_blocked_sites);
}

}  // namespace password_manager
