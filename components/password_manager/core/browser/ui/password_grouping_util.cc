// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_grouping_util.h"

#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

PasswordGroupingInfo::PasswordGroupingInfo() = default;

PasswordGroupingInfo::~PasswordGroupingInfo() = default;

PasswordGroupingInfo::PasswordGroupingInfo(const PasswordGroupingInfo& other) =
    default;

PasswordGroupingInfo::PasswordGroupingInfo(PasswordGroupingInfo&& other) =
    default;

PasswordGroupingInfo& PasswordGroupingInfo::operator=(
    const PasswordGroupingInfo& other) = default;

PasswordGroupingInfo& PasswordGroupingInfo::operator=(
    PasswordGroupingInfo&& other) = default;

// Returns a map of facet URI to group id. Create missing group id with
// password's sign-on realm that are not present in the grouped facets received.
// Store branding information for the affiliated group by updating the password
// grouping info parameter.
std::map<std::string, GroupId> MapFacetsToGroupId(
    const std::vector<GroupedFacets>& groups,
    const std::vector<std::string>& signon_realms,
    PasswordGroupingInfo& password_grouping_info) {
  int group_id_int = 1;
  std::map<std::string, GroupId> map_facet_to_group_id;
  std::set<std::string> facet_uri_in_groups;

  for (const GroupedFacets& grouped_facets : groups) {
    GroupId unique_group_id(group_id_int);
    for (const Facet& facet : grouped_facets.facets) {
      std::string facet_uri_str = facet.uri.canonical_spec();
      map_facet_to_group_id[facet_uri_str] = unique_group_id;

      // Keep track of facet URI (sign-on realm) that are already in a group.
      facet_uri_in_groups.insert(facet_uri_str);
    }

    // Store branding information for the affiliated group.
    password_grouping_info.map_group_id_to_branding_info[unique_group_id] =
        grouped_facets.branding_info;

    // Increment so it is a new id for the next group.
    group_id_int++;
  }

  // Create a group ID for the sign-on realms that are not part of any grouped
  // facets.
  for (const std::string& signon_realm : signon_realms) {
    FacetURI uri(FacetURI::FromPotentiallyInvalidSpec(signon_realm));
    if (facet_uri_in_groups.find(uri.canonical_spec()) ==
        facet_uri_in_groups.end()) {
      map_facet_to_group_id[uri.canonical_spec()] = GroupId(group_id_int);
      facet_uri_in_groups.insert(uri.canonical_spec());

      group_id_int++;
    }
  }

  return map_facet_to_group_id;
}

PasswordGroupingInfo GroupPasswords(
    const std::vector<GroupedFacets>& groups,
    const std::multimap<std::string, PasswordForm>&
        sort_key_to_password_forms) {
  PasswordGroupingInfo password_grouping_info;

  // Extract all sign-on realms to group.
  std::vector<std::string> signon_realms;
  for (auto const& element : sort_key_to_password_forms) {
    const PasswordForm& form = element.second;
    // Do not group blocked by user password forms.
    if (form.blocked_by_user) {
      password_grouping_info.blocked_sites.emplace_back(form);
    } else {
      signon_realms.push_back(form.signon_realm);
    }
  }

  // Construct map to keep track of facet URI to group id mapping.
  std::map<std::string, GroupId> map_facet_to_group_id =
      MapFacetsToGroupId(groups, signon_realms, password_grouping_info);

  // Construct a map to keep track of group id to a map of credential groups
  // to password form.
  for (auto const& element : sort_key_to_password_forms) {
    PasswordForm form = element.second;

    // Do not group blocked by user password forms.
    if (form.blocked_by_user)
      continue;

    FacetURI uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
    GroupId group_id = map_facet_to_group_id[uri.canonical_spec()];

    UsernamePasswordKey key(CreateUsernamePasswordSortKey(form));
    password_grouping_info.map_group_id_to_forms[group_id][key].push_back(
        std::move(form));

    // Store group id for sign-on realm.
    SignonRealm signon_realm(uri.canonical_spec());
    password_grouping_info.map_signon_realm_to_group_id[signon_realm] =
        group_id;
  }

  return password_grouping_info;
}

std::vector<AffiliatedGroup> GetAffiliatedGroupsWithGroupingInfo(
    const PasswordGroupingInfo& password_grouping_info) {
  std::vector<AffiliatedGroup> affiliated_groups;
  // Key: Group id | Value: map of vectors of password forms.
  for (auto const& it : password_grouping_info.map_group_id_to_forms) {
    AffiliatedGroup affiliated_group;

    // Add branding information to the affiliated group.
    auto it2 =
        password_grouping_info.map_group_id_to_branding_info.find(it.first);
    // TODO(crbug.com/1354196): Implement fallback.
    if (it2 != password_grouping_info.map_group_id_to_branding_info.end()) {
      affiliated_group.SetBrandingInfo(it2->second);
    }

    // Key: Username-password key | Value: vector of password forms.
    for (auto const& it3 : it.second) {
      affiliated_group.AddCredential(CredentialUIEntry(it3.second));
    }
    affiliated_groups.push_back(std::move(affiliated_group));
  }
  return affiliated_groups;
}

}  // namespace password_manager
