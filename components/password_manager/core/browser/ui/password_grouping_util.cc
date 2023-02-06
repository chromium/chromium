// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_grouping_util.h"

#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

std::string GetSignonRealm(const PasswordForm& form) {
  if (form.IsFederatedCredential()) {
    return base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
               url::Origin::Create(form.url),
               url_formatter::SchemeDisplay::SHOW)) +
           '/';
  }
  return form.signon_realm;
}

}  // namespace

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

std::vector<PasswordForm> PasswordGroupingInfo::GetPasswordFormsVector(
    const CredentialUIEntry& credential) const {
  std::vector<PasswordForm> forms;

  // Verify if the credential is in blocked sites first.
  if (credential.blocked_by_user) {
    for (const auto& blocked_site : blocked_sites) {
      if (credential.GetFirstSignonRealm() == blocked_site.signon_realm) {
        forms.push_back(blocked_site);
      }
    }
    return forms;
  }

  auto group_id_iterator = map_signon_realm_to_group_id.find(
      SignonRealm(credential.GetFirstSignonRealm()));
  if (group_id_iterator == map_signon_realm_to_group_id.end()) {
    return forms;
  }
  GroupId group_id = group_id_iterator->second;
  auto group_iterator = map_group_id_to_forms.find(group_id);
  if (group_iterator == map_group_id_to_forms.end()) {
    return forms;
  }
  std::map<UsernamePasswordKey, std::vector<PasswordForm>> map =
      group_iterator->second;
  auto forms_iterator =
      map.find(UsernamePasswordKey(CreateUsernamePasswordSortKey(credential)));
  if (forms_iterator != map.end()) {
    forms = forms_iterator->second;
  }
  return forms;
}

std::vector<CredentialUIEntry> PasswordGroupingInfo::GetBlockedSites() const {
  std::vector<CredentialUIEntry> results(blocked_sites.size());
  std::transform(blocked_sites.begin(), blocked_sites.end(), results.begin(),
                 [](const PasswordForm& password_form) {
                   return CredentialUIEntry(password_form);
                 });
  // Sort blocked sites.
  std::sort(results.begin(), results.end());

  return results;
}

FacetBrandingInfo CreateBrandingInfoFromFacetURI(
    const CredentialUIEntry& credential) {
  FacetBrandingInfo branding_info;
  if (IsValidAndroidFacetURI(credential.GetFirstSignonRealm())) {
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(credential.GetFirstSignonRealm());
    branding_info.name = SplitByDotAndReverse(facet_uri.android_package_name());

    // TODO(crbug.com/1355956): Handle Android App icon URL.
    return branding_info;
  }
  branding_info.name = GetShownOrigin(credential);
  // TODO(crbug.com/1355956): Handle default icon URL.
  return branding_info;
}

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
      std::string facet_uri_str = facet.uri.canonical_spec() + "/";
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
    GroupId unique_group_id(group_id_int);
    if (facet_uri_in_groups.find(signon_realm) == facet_uri_in_groups.end()) {
      map_facet_to_group_id[signon_realm] = unique_group_id;
      facet_uri_in_groups.insert(signon_realm);

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
      signon_realms.push_back(GetSignonRealm(form));
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
    std::string signon_realm = GetSignonRealm(form);

    GroupId group_id = map_facet_to_group_id[signon_realm];

    UsernamePasswordKey key(CreateUsernamePasswordSortKey(form));
    password_grouping_info.map_group_id_to_forms[group_id][key].push_back(
        std::move(form));

    // Store group id for sign-on realm.
    password_grouping_info
        .map_signon_realm_to_group_id[SignonRealm(signon_realm)] = group_id;
  }

  return password_grouping_info;
}

std::vector<AffiliatedGroup> GetAffiliatedGroupsWithGroupingInfo(
    const PasswordGroupingInfo& password_grouping_info) {
  std::vector<AffiliatedGroup> affiliated_groups;
  // Key: Group id | Value: map of vectors of password forms.
  for (auto const& it : password_grouping_info.map_group_id_to_forms) {
    AffiliatedGroup affiliated_group;

    // Key: Username-password key | Value: vector of password forms.
    for (auto const& it3 : it.second) {
      CredentialUIEntry credential(it3.second);
      affiliated_group.AddCredential(std::move(credential));
    }

    // Add branding information to the affiliated group.
    auto it2 =
        password_grouping_info.map_group_id_to_branding_info.find(it.first);
    if (it2 != password_grouping_info.map_group_id_to_branding_info.end()) {
      affiliated_group.SetBrandingInfo(it2->second);
    }
    // If the branding information is missing, create a default one with the
    // sign-on realm.
    if (affiliated_group.GetDisplayName().empty() &&
        affiliated_group.GetIconURL().is_empty()) {
      affiliated_group.SetBrandingInfo(CreateBrandingInfoFromFacetURI(
          *affiliated_group.GetCredentials().begin()));
    }
    affiliated_groups.push_back(std::move(affiliated_group));
  }
  return affiliated_groups;
}

}  // namespace password_manager
