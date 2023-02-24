// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_GROUPING_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_GROUPING_UTIL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/affiliated_group.h"

namespace password_manager {

class AffiliationService;

// Helper objects which handles passwords grouping. There are two levels of
// grouping: firstly passwords with affiliated |signon_realm| are grouped
// together which corresponds to |AffiliatedGroup|. Withing these groups
// passwords are grouped by username/password pair and are referred as
// |CredentialUIEntry|. So PasswordForms with affiliated signon_realms and
// matching username/password are considered a single credential. Blocked
// websites aren't grouped at all.
class PasswordsGrouper {
 public:
  explicit PasswordsGrouper(AffiliationService* affiliation_service);
  ~PasswordsGrouper();

  // Apply grouping algorithm to credentials. The grouping algorithm group
  // together credentials with the same username and password within the same
  // affiliated group. For example, we have credential from "facebook.com" and
  // "m.facebook.com" that have the same username and password. These
  // credentials are part of the same affiliated group so they will be grouped
  // together.
  // |forms| PasswordForms to be grouped.
  // |callback| is called after the grouping is finished.
  void GroupPasswords(std::vector<PasswordForm> forms,
                      base::OnceClosure callback);

  // Returns a list of affiliated groups created with the password grouping
  // info.
  std::vector<AffiliatedGroup> GetAffiliatedGroupsWithGroupingInfo() const;

  // Returns all the credentials (excluding blocked sites) in a vector.
  std::vector<CredentialUIEntry> GetAllCredentials() const;

  // Returns blocked sites.
  std::vector<CredentialUIEntry> GetBlockedSites() const;

  // Returns PasswordForm corresponding to 'credential'.
  std::vector<PasswordForm> GetPasswordFormsFor(
      const CredentialUIEntry& credential) const;

  void ClearCache();

 private:
  using SignonRealm = base::StrongAlias<class SignonRealmTag, std::string>;
  using GroupId = base::StrongAlias<class GroupIdTag, int>;
  using UsernamePasswordKey =
      base::StrongAlias<class UsernamePasswordKeyTag, std::string>;

  // Returns a map of facet URI to group id. Stores branding information for the
  // affiliated group by updating |map_group_id_to_branding_info|.
  std::map<std::string, GroupId> MapFacetsToGroupId(
      const std::vector<GroupedFacets>& groups);

  void GroupPasswordsImpl(std::vector<PasswordForm> forms,
                          const std::vector<GroupedFacets>& groups);

  void InitializePSLExtensionList(std::vector<std::string> psl_extension_list);

  raw_ptr<AffiliationService> affiliation_service_;

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
  std::vector<PasswordForm> blocked_sites;

  // The set of domains that the server uses as an extension to the PSL.
  base::flat_set<std::string> psl_extensions_;

  base::WeakPtrFactory<PasswordsGrouper> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_GROUPING_UTIL_H_
