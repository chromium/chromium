// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/passwords_grouper.h"

#include <algorithm>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/features.h"
#include "base/memory/raw_span.h"
#include "base/memory/safety_checks.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

// Kill switch guarding an investigation of PasswordGrouper double-free or
// out-of-bounds writes. Behind a flag in case it finds too many issues and is
// a stability risk, or there's an unexpected performance impact.
BASE_FEATURE(kPasswordsGrouperHeapIntegrityKillSwitch,
             "PasswordsGrouperHeapIntegrityKillSwitchv2",
             base::FEATURE_ENABLED_BY_DEFAULT);

using affiliations::FacetBrandingInfo;
using affiliations::FacetURI;
using affiliations::GroupedFacets;

constexpr char kDefaultFallbackIconUrl[] = "https://t1.gstatic.com/faviconV2";
constexpr char kFallbackIconQueryParams[] =
    "client=PASSWORD_MANAGER&type=FAVICON&fallback_opts=TYPE,SIZE,URL,"
    "TOP_DOMAIN&size=32&url=";
constexpr char kDefaultAndroidIcon[] =
    "https://www.gstatic.com/images/branding/product/1x/play_apps_32dp.png";

FacetBrandingInfo CreateBrandingInfoFromFacetURI(
    const CredentialUIEntry& credential,
    const base::flat_set<std::string>& psl_extensions) {
  FacetBrandingInfo branding_info;
  FacetURI facet_uri =
      FacetURI::FromPotentiallyInvalidSpec(credential.GetFirstSignonRealm());
  if (facet_uri.IsValidAndroidFacetURI()) {
    branding_info.name = facet_uri.GetAndroidPackageDisplayName();
    branding_info.icon_url = GURL(kDefaultAndroidIcon);
    return branding_info;
  }
  std::string group_name = affiliations::GetExtendedTopLevelDomain(
      credential.GetURL(), psl_extensions);
  if (group_name.empty()) {
    group_name =
        credential.GetURL().is_valid()
            ? base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
                  credential.GetURL()))
            : facet_uri.potentially_invalid_spec();
  }
  branding_info.name = group_name;

  GURL::Replacements replacements;
  std::string query =
      kFallbackIconQueryParams +
      base::EscapeQueryParamValue(credential.GetURL().possibly_invalid_spec(),
                                  /*use_plus=*/false);
  replacements.SetQueryStr(query);
  branding_info.icon_url =
      GURL(kDefaultFallbackIconUrl).ReplaceComponents(replacements);
  return branding_info;
}

std::string CreateUsernamePasswordSortKey(const CredentialUIEntry& credential) {
  std::string key;
  // The origin isn't taken into account for normal credentials since we want to
  // group them together.
  const char kSortKeyPartsSeparator = ' ';
  if (!credential.blocked_by_user) {
    key += base::UTF16ToUTF8(credential.username) + kSortKeyPartsSeparator +
           base::UTF16ToUTF8(credential.password);

    key += kSortKeyPartsSeparator;
    if (credential.federation_origin.IsValid()) {
      key += credential.federation_origin.host();
    } else {
      key += kSortKeyPartsSeparator;
    }
  } else {
    // Key for blocked by user credential since it does not store username and
    // password. These credentials are not grouped together.
    key = credential.GetAffiliatedDomains()[0].name;
  }
  return key;
}

// Presents a sorted view of a span of `PasskeyCredential`s, ordered by
// increasing user name.
class SortedPasskeysView {
 public:
  class iterator {
   public:
    iterator(size_t i, const SortedPasskeysView* sorted)
        : i_(i), sorted_(sorted) {}

    friend bool operator==(const iterator&, const iterator&) = default;

    void operator++() { i_++; }
    const PasskeyCredential& operator*() {
      return sorted_->passkeys_[sorted_->sorted_indexes_[i_]];
    }

   private:
    size_t i_ = 0;
    const raw_ptr<const SortedPasskeysView> sorted_;
  };

  explicit SortedPasskeysView(
      const base::span<const PasskeyCredential>& passkeys)
      : passkeys_(passkeys) {
    sorted_indexes_.reserve(passkeys_.size());
    for (size_t i = 0; i < passkeys_.size(); i++) {
      sorted_indexes_.push_back(i);
    }
    std::ranges::sort(sorted_indexes_, [this](size_t a, size_t b) {
      return passkeys_[a].username() < passkeys_[b].username();
    });
  }

  iterator begin() const { return iterator(0, this); }
  iterator end() const { return iterator(passkeys_.size(), this); }

 private:
  const base::raw_span<const PasskeyCredential> passkeys_;
  std::vector<size_t> sorted_indexes_;
};

}  // namespace

PasswordsGrouper::Credentials::Credentials() = default;
PasswordsGrouper::Credentials::~Credentials() = default;

PasswordsGrouper::PasswordsGrouper(
    affiliations::AffiliationService* affiliation_service) {
  DCHECK(affiliation_service);
  affiliation_service_ = affiliation_service->AsWeakPtr();
  affiliation_service_->GetPSLExtensions(
      base::BindOnce(&PasswordsGrouper::InitializePSLExtensionList,
                     weak_ptr_factory_.GetWeakPtr()));
}
PasswordsGrouper::~PasswordsGrouper() = default;

void PasswordsGrouper::GroupCredentials(
    std::vector<StoredCredential> stored_credentials,
    std::vector<PasskeyCredential> passkeys,
    base::OnceClosure callback) {
  if (!affiliation_service_) {
    return;
  }

  // Convert forms to Facets.
  std::vector<FacetURI> facets;
  facets.reserve(stored_credentials.size());
  for (const auto& form : stored_credentials) {
    // Blocked forms aren't grouped.
    if (!form.blocked_by_user) {
      facets.emplace_back(
          FacetURI::FromPotentiallyInvalidSpec(GetFacetRepresentation(form)));
    }
  }

  // Convert passkey relying party IDs to Facets.
  for (const auto& passkey : passkeys) {
    facets.emplace_back(
        FacetURI::FromPotentiallyInvalidSpec(GetFacetRepresentation(passkey)));
  }

  affiliations::AffiliationService::GroupsCallback group_callback =
      base::BindOnce(&PasswordsGrouper::GroupCredentialsImpl,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(stored_credentials), std::move(passkeys));

  // Before grouping passwords merge related groups. After grouping is finished
  // invoke |callback|.
  affiliation_service_->GetGroupingInfo(
      std::move(facets),
      base::BindOnce(&affiliations::MergeRelatedGroups, psl_extensions_)
          .Then(std::move(group_callback))
          .Then(std::move(callback)));
}

std::vector<AffiliatedGroup>
PasswordsGrouper::GetAffiliatedGroupsWithGroupingInfo() const {
  std::vector<AffiliatedGroup> affiliated_groups;
  for (auto const& [group_id, affiliated_group] :
       map_group_id_to_credentials_) {
    CheckHeapIntegrity();
    // Convert each credential into CredentialUIEntry.
    std::vector<CredentialUIEntry> credentials;
    for (auto const& [username_password_key, credentials_in_group] :
         affiliated_group.stored_credentials) {
      credentials.emplace_back(credentials_in_group);
    }
    for (auto const& passkey : SortedPasskeysView(affiliated_group.passkeys)) {
      credentials.emplace_back(passkey);
    }

    // Add branding information to the affiliated group.
    FacetBrandingInfo brandingInfo;
    auto branding_iterator = map_group_id_to_branding_info_.find(group_id);
    if (branding_iterator != map_group_id_to_branding_info_.end()) {
      brandingInfo = branding_iterator->second;
    }
    // If the branding information is missing, create a default one with the
    // sign-on realm.
    if (brandingInfo.name.empty()) {
      brandingInfo =
          CreateBrandingInfoFromFacetURI(credentials[0], psl_extensions_);
    }
    affiliated_groups.emplace_back(std::move(credentials), brandingInfo);
    CheckHeapIntegrity();
  }
  // Sort affiliated groups.
  std::sort(affiliated_groups.begin(), affiliated_groups.end(),
            [](AffiliatedGroup& lhs, AffiliatedGroup& rhs) {
              std::string_view lhs_name(lhs.GetDisplayName()),
                  rhs_name(rhs.GetDisplayName());
              size_t separator_length =
                  std::string_view(url::kStandardSchemeSeparator).size();

              size_t position = lhs_name.find(url::kStandardSchemeSeparator);
              if (position != std::string::npos) {
                lhs_name = lhs_name.substr(position + separator_length);
              }

              position = rhs_name.find(url::kStandardSchemeSeparator);
              if (position != std::string::npos) {
                rhs_name = rhs_name.substr(position + separator_length);
              }

              // Compare names omitting scheme.
              return base::CompareCaseInsensitiveASCII(lhs_name, rhs_name) < 0;
            });
  CheckHeapIntegrity();
  return affiliated_groups;
}

std::vector<CredentialUIEntry> PasswordsGrouper::GetAllCredentials() const {
  std::vector<CredentialUIEntry> credentials;
  CheckHeapIntegrity();
  for (const auto& [group_id, affiliated_credentials] :
       map_group_id_to_credentials_) {
    for (const auto& [username_password_key, credentials_in_group] :
         affiliated_credentials.stored_credentials) {
      credentials.emplace_back(credentials_in_group);
    }
    for (const auto& passkey :
         SortedPasskeysView(affiliated_credentials.passkeys)) {
      credentials.emplace_back(passkey);
    }
  }
  CheckHeapIntegrity();
  return credentials;
}

std::vector<CredentialUIEntry> PasswordsGrouper::GetBlockedSites() const {
  std::vector<CredentialUIEntry> results;
  results.reserve(blocked_sites_.size());
  std::ranges::transform(blocked_sites_, std::back_inserter(results),
                         [](const auto& key_value) {
                           return CredentialUIEntry(key_value.second.front());
                         });
  // Sort blocked sites.
  std::sort(results.begin(), results.end());
  return results;
}

std::vector<StoredCredential> PasswordsGrouper::GetStoredCredentialsFor(
    const CredentialUIEntry& credential) const {
  std::vector<StoredCredential> credentials;

  // Verify if the credential is in blocked sites first.
  if (credential.blocked_by_user) {
    const std::string displayed_name =
        credential.GetAffiliatedDomains().front().name;
    const auto& iterator = blocked_sites_.find(displayed_name);
    if (iterator != blocked_sites_.end()) {
      for (const auto& stored_credential : iterator->second) {
        credentials.push_back(CloneStoredCredential(stored_credential));
      }
      return credentials;
    }
    return credentials;
  }

  // Get group id based on signon_realm.
  auto group_id_iterator = map_signon_realm_to_group_id_.find(
      SignonRealm(credential.GetFirstSignonRealm()));
  if (group_id_iterator == map_signon_realm_to_group_id_.end()) {
    return {};
  }

  // Get all username/password pairs related to this group.
  GroupId group_id = group_id_iterator->second;
  CheckHeapIntegrity();
  auto group_iterator = map_group_id_to_credentials_.find(group_id);
  if (group_iterator == map_group_id_to_credentials_.end()) {
    return {};
  }

  // Get all stored credentials with matching username/password.
  const std::map<UsernamePasswordKey, std::vector<StoredCredential>>&
      username_to_credentials = group_iterator->second.stored_credentials;
  auto credentials_iterator = username_to_credentials.find(
      UsernamePasswordKey(CreateUsernamePasswordSortKey(credential)));
  if (credentials_iterator == username_to_credentials.end()) {
    return {};
  }
  CheckHeapIntegrity();

  std::vector<StoredCredential> result;
  result.reserve(credentials_iterator->second.size());
  for (const auto& stored_credential : credentials_iterator->second) {
    result.push_back(CloneStoredCredential(stored_credential));
  }
  return result;
}

std::optional<PasskeyCredential> PasswordsGrouper::GetPasskeyFor(
    const CredentialUIEntry& credential) {
  // Find the group id based on the sign on realm.
  auto group_id_iterator = map_signon_realm_to_group_id_.find(
      SignonRealm(credential.GetFirstSignonRealm()));
  if (group_id_iterator == map_signon_realm_to_group_id_.end()) {
    return std::nullopt;
  }
  // Find the passkey in the group.
  const std::vector<PasskeyCredential>& passkeys =
      map_group_id_to_credentials_[group_id_iterator->second].passkeys;
  const auto passkey_it =
      std::ranges::find(passkeys, credential.passkey_credential_id,
                        &PasskeyCredential::credential_id);
  if (passkey_it == passkeys.end()) {
    return std::nullopt;
  }
  return *passkey_it;
}

void PasswordsGrouper::ClearCache() {
  map_signon_realm_to_group_id_.clear();
  map_group_id_to_branding_info_.clear();
  CheckHeapIntegrity();
  map_group_id_to_credentials_.clear();
  CheckHeapIntegrity();
  blocked_sites_.clear();
}

void PasswordsGrouper::CheckHeapIntegrity() const {
  if (base::FeatureList::IsEnabled(kPasswordsGrouperHeapIntegrityKillSwitch)) {
    return;
  }
  for (const auto& pair : map_group_id_to_credentials_) {
    const Credentials& credentials = pair.second;
    for (const auto& credentials_pair : credentials.stored_credentials) {
      const std::vector<StoredCredential>& stored_credentials_vector =
          credentials_pair.second;
      for (const auto& stored_credential : stored_credentials_vector) {
        base::CheckHeapIntegrity(&stored_credential);
      }
    }
    for (const auto& passkey : credentials.passkeys) {
      base::CheckHeapIntegrity(&passkey);
    }
  }
}

void PasswordsGrouper::GroupCredentialsImpl(
    std::vector<StoredCredential> stored_credentials,
    std::vector<PasskeyCredential> passkeys,
    const std::vector<GroupedFacets>& groups) {
  ClearCache();
  // Construct map to keep track of facet URI to group id mapping.
  std::map<std::string, GroupId> map_facet_to_group_id =
      MapFacetsToGroupId(groups);

  // Construct a map to keep track of group id to a map of credential groups
  // to stored credential.
  for (auto& stored_credential : stored_credentials) {
    // Do not group blocked by user stored credentials.
    if (stored_credential.blocked_by_user) {
      CredentialUIEntry credential(stored_credential);
      std::string displayed_name =
          credential.GetAffiliatedDomains().front().name;
      blocked_sites_[displayed_name].push_back(std::move(stored_credential));
      continue;
    }
    std::string facet_uri = GetFacetRepresentation(stored_credential);

    DCHECK(map_facet_to_group_id.contains(facet_uri));
    GroupId group_id = map_facet_to_group_id[facet_uri];

    // Store group id for sign-on realm.
    map_signon_realm_to_group_id_[SignonRealm(stored_credential.signon_realm)] =
        group_id;

    // Store credential for username/password key.
    UsernamePasswordKey key(
        CreateUsernamePasswordSortKey(CredentialUIEntry(stored_credential)));
    CheckHeapIntegrity();
    map_group_id_to_credentials_[group_id].stored_credentials[key].push_back(
        std::move(stored_credential));
    CheckHeapIntegrity();
  }

  for (auto& passkey : passkeys) {
    // Group passkeys.
    std::string facet_uri = GetFacetRepresentation(passkey);
    GroupId group_id = map_facet_to_group_id[facet_uri];
    map_signon_realm_to_group_id_[SignonRealm(facet_uri)] = group_id;
    CheckHeapIntegrity();
    map_group_id_to_credentials_[group_id].passkeys.push_back(
        std::move(passkey));
    CheckHeapIntegrity();
  }
}

std::map<std::string, PasswordsGrouper::GroupId>
PasswordsGrouper::MapFacetsToGroupId(const std::vector<GroupedFacets>& groups) {
  int group_id_int = 1;
  std::map<std::string, GroupId> map_facet_to_group_id;
  std::set<std::string> facet_uri_in_groups;

  for (const GroupedFacets& grouped_facets : groups) {
    GroupId unique_group_id(group_id_int);
    for (const affiliations::Facet& facet : grouped_facets.facets) {
      std::string facet_uri_str = facet.uri.potentially_invalid_spec();
      map_facet_to_group_id[facet_uri_str] = unique_group_id;

      // Keep track of facet URI (sign-on realm) that are already in a group.
      facet_uri_in_groups.insert(facet_uri_str);
    }

    // Store branding information for the affiliated group.
    map_group_id_to_branding_info_[unique_group_id] =
        grouped_facets.branding_info;

    // Increment so it is a new id for the next group.
    group_id_int++;
  }

  return map_facet_to_group_id;
}

void PasswordsGrouper::InitializePSLExtensionList(
    std::vector<std::string> psl_extension_list) {
  CheckHeapIntegrity();
  psl_extensions_ = base::flat_set<std::string>(std::move(psl_extension_list));
  CheckHeapIntegrity();
}

std::string GetFacetRepresentation(const StoredCredential& credential) {
  FacetURI facet =
      FacetURI::FromPotentiallyInvalidSpec(credential.signon_realm);
  // Return result for android credentials immediately.
  if (facet.IsValidAndroidFacetURI()) {
    return facet.potentially_invalid_spec();
  }
  GURL url;
  // For federated credentials use url. For everything else try to parse signon
  // realm as GURL.
  if (credential.federation_origin.IsValid()) {
    url = credential.url;
  } else {
    url = GURL(credential.signon_realm);
  }

  // Strip path and everything after that.
  std::string scheme_and_authority = url.GetWithEmptyPath().spec();

  // If something went wrong (signon_realm is not a valid GURL), use signon
  // realm as it is.
  if (scheme_and_authority.empty()) {
    scheme_and_authority = credential.signon_realm;
  }
  return FacetURI::FromPotentiallyInvalidSpec(scheme_and_authority)
      .potentially_invalid_spec();
}

std::string GetFacetRepresentation(const PasskeyCredential& passkey) {
  std::string as_url = base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, passkey.rp_id()});
  return FacetURI::FromPotentiallyInvalidSpec(as_url)
      .potentially_invalid_spec();
}

}  // namespace password_manager
