// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_GROUPING_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_GROUPING_UTIL_H_

#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/affiliated_group.h"

namespace password_manager {

using SignonRealm = base::StrongAlias<class SignonRealmTag, std::string>;
using GroupId = base::StrongAlias<class GroupIdTag, int>;
using UsernamePasswordKey =
    base::StrongAlias<class UsernamePasswordKeyTag, std::string>;

// Structure used to store password grouping data structures used for the
// grouping algorithm.
struct PasswordGroupingInfo {
  PasswordGroupingInfo();
  ~PasswordGroupingInfo();
  PasswordGroupingInfo(const PasswordGroupingInfo& other);
  PasswordGroupingInfo(PasswordGroupingInfo&& other);
  PasswordGroupingInfo& operator=(const PasswordGroupingInfo& other);
  PasswordGroupingInfo& operator=(PasswordGroupingInfo&& other);

  // Structure used to keep track of the mapping between the credential's
  // sign-on realm and the group id.
  std::map<SignonRealm, GroupId> map_signon_realm_to_group_id;

  // Structure used to keep track of the mapping between the group id and the
  // grouped facet's branding information.
  std::map<GroupId, FacetBrandingInfo> map_group_id_to_branding_info;

  // Structure used to keep track of the mapping between a group id and the
  // grouped by username-password key password forms.
  std::map<GroupId, std::map<UsernamePasswordKey, std::vector<PasswordForm>>>
      map_group_id_to_forms;

  // Structure to keep track of the blocked sites by user. They are not grouped
  // into affiliated groups.
  std::vector<password_manager::CredentialUIEntry> blocked_sites;
};

// Apply grouping algorithm to credentials. The grouping algorithm group
// together credentials with the same username and password under the same
// affiliated group. For example, we have credential from "facebook.com" and
// "m.facebook.com" that have the same username and password. These are
// credentials are part of the same affiliated group so they will be grouped
// together. This method will create the password grouping info which contains
// the data structures used to create the list of affiliated groups.
PasswordGroupingInfo GroupPasswords(
    const std::vector<GroupedFacets>& groups,
    const std::multimap<std::string, PasswordForm>& sort_key_to_password_forms);

// Returns a list of affiliated groups created with the password grouping info.
std::vector<AffiliatedGroup> GetAffiliatedGroupsWithGroupingInfo(
    const PasswordGroupingInfo& password_grouping_info);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_GROUPING_UTIL_H_
