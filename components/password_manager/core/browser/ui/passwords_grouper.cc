// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
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

PasswordsGrouper::PasswordsGrouper() = default;
PasswordsGrouper::~PasswordsGrouper() = default;

void PasswordsGrouper::GroupPasswords(
    const std::vector<GroupedFacets>& groups,
    const std::multimap<std::string, PasswordForm>&
        sort_key_to_password_forms) {
  password_grouping_info_.clear();

  // Extract all sign-on realms to group.
  std::vector<std::string> signon_realms;
  for (auto const& element : sort_key_to_password_forms) {
    const PasswordForm& form = element.second;
    // Do not group blocked by user password forms.
    if (form.blocked_by_user) {
      password_grouping_info_.blocked_sites.emplace_back(form);
    } else {
      signon_realms.push_back(GetSignonRealm(form));
    }
  }

  // Construct map to keep track of facet URI to group id mapping.
  std::map<std::string, GroupId> map_facet_to_group_id =
      MapFacetsToGroupId(groups, signon_realms, password_grouping_info_);

  // Construct a map to keep track of group id to a map of credential groups
  // to password form.
  for (auto const& element : sort_key_to_password_forms) {
    PasswordForm form = element.second;

    // Do not group blocked by user password forms.
    if (form.blocked_by_user) {
      continue;
    }
    std::string signon_realm = GetSignonRealm(form);

    GroupId group_id = map_facet_to_group_id[signon_realm];

    UsernamePasswordKey key(CreateUsernamePasswordSortKey(form));
    password_grouping_info_.map_group_id_to_forms[group_id][key].push_back(
        std::move(form));

    // Store group id for sign-on realm.
    password_grouping_info_
        .map_signon_realm_to_group_id[SignonRealm(signon_realm)] = group_id;
  }
}

std::vector<AffiliatedGroup>
PasswordsGrouper::GetAffiliatedGroupsWithGroupingInfo() const {
  std::vector<AffiliatedGroup> affiliated_groups;
  for (auto const& [group_id, affiliated_group] :
       password_grouping_info_.map_group_id_to_forms) {
    std::vector<CredentialUIEntry> credentials;

    // Convert each vector<PasswordForm> into CredentialUIEntry.
    for (auto const& [username_password_key, forms] : affiliated_group) {
      credentials.emplace_back(forms);
    }

    // Add branding information to the affiliated group.
    FacetBrandingInfo brandingInfo;
    auto branding_iterator =
        password_grouping_info_.map_group_id_to_branding_info.find(group_id);
    if (branding_iterator !=
        password_grouping_info_.map_group_id_to_branding_info.end()) {
      brandingInfo = branding_iterator->second;
    }
    // If the branding information is missing, create a default one with the
    // sign-on realm.
    if (brandingInfo.name.empty()) {
      brandingInfo = CreateBrandingInfoFromFacetURI(credentials[0]);
    }
    affiliated_groups.emplace_back(std::move(credentials), brandingInfo);
  }
  // Sort affiliated groups.
  std::sort(affiliated_groups.begin(), affiliated_groups.end(),
            [](const AffiliatedGroup& lhs, const AffiliatedGroup& rhs) {
              return lhs.GetDisplayName() < rhs.GetDisplayName();
            });
  return affiliated_groups;
}

std::vector<CredentialUIEntry> PasswordsGrouper::GetAllCredentials() const {
  std::vector<CredentialUIEntry> credentials;
  for (const auto& [group_id, affiliated_credentials] :
       password_grouping_info_.map_group_id_to_forms) {
    for (const auto& [username_password_key, forms] : affiliated_credentials) {
      credentials.emplace_back(forms);
    }
  }
  return credentials;
}

std::vector<CredentialUIEntry> PasswordsGrouper::GetBlockedSites() const {
  std::vector<CredentialUIEntry> results;
  results.reserve(password_grouping_info_.blocked_sites.size());
  base::ranges::transform(password_grouping_info_.blocked_sites,
                          std::back_inserter(results),
                          [](const PasswordForm& password_form) {
                            return CredentialUIEntry(password_form);
                          });
  // Sort blocked sites.
  std::sort(results.begin(), results.end());
  return results;
}

std::vector<PasswordForm> PasswordsGrouper::GetPasswordFormsFor(
    const CredentialUIEntry& credential) const {
  std::vector<PasswordForm> forms;

  // Verify if the credential is in blocked sites first.
  if (credential.blocked_by_user) {
    for (const auto& blocked_site : password_grouping_info_.blocked_sites) {
      if (credential.GetFirstSignonRealm() == blocked_site.signon_realm) {
        forms.push_back(blocked_site);
      }
    }
    return {};
  }

  // Get group id based on signon_realm.
  auto group_id_iterator =
      password_grouping_info_.map_signon_realm_to_group_id.find(
          SignonRealm(credential.GetFirstSignonRealm()));
  if (group_id_iterator ==
      password_grouping_info_.map_signon_realm_to_group_id.end()) {
    return {};
  }

  // Get all username/password pairs related to this group.
  GroupId group_id = group_id_iterator->second;
  auto group_iterator =
      password_grouping_info_.map_group_id_to_forms.find(group_id);
  if (group_iterator == password_grouping_info_.map_group_id_to_forms.end()) {
    return {};
  }

  // Get all password forms with matching username/password.
  const std::map<UsernamePasswordKey, std::vector<PasswordForm>>&
      username_to_forms = group_iterator->second;
  auto forms_iterator = username_to_forms.find(
      UsernamePasswordKey(CreateUsernamePasswordSortKey(credential)));
  if (forms_iterator == username_to_forms.end()) {
    return {};
  }

  return forms_iterator->second;
}

}  // namespace password_manager
