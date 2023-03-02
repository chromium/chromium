// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include "base/check_op.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_util.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

// Returns signon_realm for regular forms and formatted url for federated forms.
std::string GetFacetRepresentation(const PasswordForm& form) {
  std::string result = form.signon_realm;
  if (form.IsFederatedCredential()) {
    result = base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
        form.url, url_formatter::SchemeDisplay::SHOW));
  }
  return FacetURI::FromPotentiallyInvalidSpec(result)
      .potentially_invalid_spec();
}

// An implementation of the disjoint-set data structure
// (https://en.wikipedia.org/wiki/Disjoint-set_data_structure). This
// implementation uses the path compression and union by rank optimizations,
// achieving near-constant runtime on all operations.
//
// This data structure allows to keep track of disjoin sets. Constructor accepts
// number of elements and initially each element represent an individual set.
// Later by calling MergeSets corresponding sets are merged together.
// Example usage:
//   DisjointSet disjoint_set(5);
//   disjoint_set.GetDisjointSets(); // Returns {{0}, {1}, {2}, {3}, {4}}
//   disjoint_set.MergeSets(0, 2);
//   disjoint_set.GetDisjointSets(); // Returns {{0, 2}, {1}, {3}, {4}}
//   disjoint_set.MergeSets(2, 4);
//   disjoint_set.GetDisjointSets(); // Returns {{0, 2, 4}, {1}, {3}}
class DisjointSet {
 public:
  explicit DisjointSet(size_t size) : parent_id_(size), ranks_(size, 0) {
    for (size_t i = 0; i < size; i++) {
      parent_id_[i] = i;
    }
  }

  // Merges two sets based on their rank. Set with higher rank becomes a parent
  // for another set.
  void MergeSets(int set1, int set2) {
    set1 = GetRoot(set1);
    set2 = GetRoot(set2);
    if (set1 == set2) {
      return;
    }

    // Update parent based on rank.
    if (ranks_[set1] > ranks_[set2]) {
      parent_id_[set2] = set1;
    } else {
      parent_id_[set1] = set2;
      // if ranks were equal increment by one new root's rank.
      if (ranks_[set1] == ranks_[set2]) {
        ranks_[set2]++;
      }
    }
  }

  // Returns disjoin sets after merging. It's guarantee that the result will
  // hold all elements.
  std::vector<std::vector<int>> GetDisjointSets() {
    std::vector<std::vector<int>> disjoint_sets(parent_id_.size());
    for (size_t i = 0; i < parent_id_.size(); i++) {
      // Append all elements to the root.
      int root = GetRoot(i);
      disjoint_sets[root].push_back(i);
    }
    // Clear empty sets.
    base::EraseIf(disjoint_sets, [](const auto& set) { return set.empty(); });
    return disjoint_sets;
  }

 private:
  // Returns root for a given element.
  int GetRoot(int index) {
    if (index == parent_id_[index]) {
      return index;
    }
    // To speed up future lookups flatten the tree along the way.
    return parent_id_[index] = GetRoot(parent_id_[index]);
  }

  // Vector where element at i'th position holds a parent for i.
  std::vector<int> parent_id_;

  // Upper bound depth of a tree for i'th element.
  std::vector<size_t> ranks_;
};

// This functions merges groups together if:
// * the same facet is present in both groups
// * main domain of the facets matches
std::vector<GroupedFacets> MergeRelatedGroups(
    const base::flat_set<std::string>& psl_extensions,
    const std::vector<GroupedFacets>& groups) {
  DisjointSet unions(groups.size());
  std::map<std::string, int> main_domain_to_group;

  for (size_t i = 0; i < groups.size(); i++) {
    for (auto& facet : groups[i].facets) {
      if (facet.uri.IsValidAndroidFacetURI()) {
        continue;
      }

      // If domain is empty - compute it manually.
      std::string main_domain =
          facet.main_domain.empty()
              ? password_manager_util::GetExtendedTopLevelDomain(
                    GURL(facet.uri.potentially_invalid_spec()), psl_extensions)
              : facet.main_domain;

      if (main_domain.empty()) {
        continue;
      }

      auto it = main_domain_to_group.find(main_domain);
      if (it == main_domain_to_group.end()) {
        main_domain_to_group[main_domain] = i;
        continue;
      }
      unions.MergeSets(i, it->second);
    }
  }

  std::vector<GroupedFacets> result;
  for (const auto& merged_groups : unions.GetDisjointSets()) {
    GroupedFacets group;
    for (int group_id : merged_groups) {
      // Move all the elements into a new vector.
      group.facets.insert(group.facets.end(), groups[group_id].facets.begin(),
                          groups[group_id].facets.end());
      // Use non-empty name for a combined group.
      if (!groups[group_id].branding_info.icon_url.is_empty()) {
        group.branding_info = groups[group_id].branding_info;
      }
    }

    result.push_back(std::move(group));
  }
  return result;
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

}  // namespace

PasswordsGrouper::PasswordsGrouper(AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service) {
  DCHECK(affiliation_service_);
  affiliation_service_->GetPSLExtensions(
      base::BindOnce(&PasswordsGrouper::InitializePSLExtensionList,
                     weak_ptr_factory_.GetWeakPtr()));
}
PasswordsGrouper::~PasswordsGrouper() = default;

void PasswordsGrouper::GroupPasswords(std::vector<PasswordForm> forms,
                                      base::OnceClosure callback) {
  // Convert forms to Facets.
  std::vector<FacetURI> facets;
  facets.reserve(forms.size());
  for (const auto& form : forms) {
    // Blocked forms aren't grouped.
    if (!form.blocked_by_user) {
      facets.emplace_back(
          FacetURI::FromPotentiallyInvalidSpec(GetFacetRepresentation(form)));
    }
  }

  AffiliationService::GroupsCallback group_callback =
      base::BindOnce(&PasswordsGrouper::GroupPasswordsImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(forms));

  // Before grouping passwords merge related groups. After grouping is finished
  // invoke |callback|.
  affiliation_service_->GetGroupingInfo(
      std::move(facets), base::BindOnce(&MergeRelatedGroups, psl_extensions_)
                             .Then(std::move(group_callback))
                             .Then(std::move(callback)));
}

std::vector<AffiliatedGroup>
PasswordsGrouper::GetAffiliatedGroupsWithGroupingInfo() const {
  std::vector<AffiliatedGroup> affiliated_groups;
  for (auto const& [group_id, affiliated_group] : map_group_id_to_forms) {
    std::vector<CredentialUIEntry> credentials;

    // Convert each vector<PasswordForm> into CredentialUIEntry.
    for (auto const& [username_password_key, forms] : affiliated_group) {
      credentials.emplace_back(forms);
    }

    // Add branding information to the affiliated group.
    FacetBrandingInfo brandingInfo;
    auto branding_iterator = map_group_id_to_branding_info.find(group_id);
    if (branding_iterator != map_group_id_to_branding_info.end()) {
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
  for (const auto& [group_id, affiliated_credentials] : map_group_id_to_forms) {
    for (const auto& [username_password_key, forms] : affiliated_credentials) {
      credentials.emplace_back(forms);
    }
  }
  return credentials;
}

std::vector<CredentialUIEntry> PasswordsGrouper::GetBlockedSites() const {
  std::vector<CredentialUIEntry> results;
  results.reserve(blocked_sites.size());
  base::ranges::transform(blocked_sites, std::back_inserter(results),
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
    for (const auto& blocked_site : blocked_sites) {
      if (credential.GetFirstSignonRealm() == blocked_site.signon_realm) {
        forms.push_back(blocked_site);
      }
    }
    return forms;
  }

  // Get group id based on signon_realm.
  auto group_id_iterator = map_signon_realm_to_group_id.find(
      SignonRealm(credential.GetFirstSignonRealm()));
  if (group_id_iterator == map_signon_realm_to_group_id.end()) {
    return {};
  }

  // Get all username/password pairs related to this group.
  GroupId group_id = group_id_iterator->second;
  auto group_iterator = map_group_id_to_forms.find(group_id);
  if (group_iterator == map_group_id_to_forms.end()) {
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

void PasswordsGrouper::ClearCache() {
  map_signon_realm_to_group_id.clear();
  map_group_id_to_branding_info.clear();
  map_group_id_to_forms.clear();
  blocked_sites.clear();
}

void PasswordsGrouper::GroupPasswordsImpl(
    std::vector<PasswordForm> forms,
    const std::vector<GroupedFacets>& groups) {
  ClearCache();
  // Construct map to keep track of facet URI to group id mapping.
  std::map<std::string, GroupId> map_facet_to_group_id =
      MapFacetsToGroupId(groups);

  // Construct a map to keep track of group id to a map of credential groups
  // to password form.
  for (auto& form : forms) {
    // Do not group blocked by user password forms.
    if (form.blocked_by_user) {
      blocked_sites.push_back(std::move(form));
      continue;
    }
    std::string facet_uri = GetFacetRepresentation(form);

    DCHECK(map_facet_to_group_id.contains(facet_uri));
    GroupId group_id = map_facet_to_group_id[facet_uri];

    // Store group id for sign-on realm.
    map_signon_realm_to_group_id[SignonRealm(form.signon_realm)] = group_id;

    // Store form for username/password key.
    UsernamePasswordKey key(CreateUsernamePasswordSortKey(form));
    map_group_id_to_forms[group_id][key].push_back(std::move(form));
  }
}

std::map<std::string, PasswordsGrouper::GroupId>
PasswordsGrouper::MapFacetsToGroupId(const std::vector<GroupedFacets>& groups) {
  int group_id_int = 1;
  std::map<std::string, GroupId> map_facet_to_group_id;
  std::set<std::string> facet_uri_in_groups;

  for (const GroupedFacets& grouped_facets : groups) {
    GroupId unique_group_id(group_id_int);
    for (const Facet& facet : grouped_facets.facets) {
      std::string facet_uri_str = facet.uri.potentially_invalid_spec();
      map_facet_to_group_id[facet_uri_str] = unique_group_id;

      // Keep track of facet URI (sign-on realm) that are already in a group.
      facet_uri_in_groups.insert(facet_uri_str);
    }

    // Store branding information for the affiliated group.
    map_group_id_to_branding_info[unique_group_id] =
        grouped_facets.branding_info;

    // Increment so it is a new id for the next group.
    group_id_int++;
  }

  return map_facet_to_group_id;
}

void PasswordsGrouper::InitializePSLExtensionList(
    std::vector<std::string> psl_extension_list) {
  psl_extensions_ =
      base::MakeFlatSet<std::string>(std::move(psl_extension_list));
}

}  // namespace password_manager
