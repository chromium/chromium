// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_utils.h"

#include <map>
#include <ostream>
#include <string_view>

#include "base/base64.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon_stdstring.h"

namespace affiliations {

namespace {

// The scheme used for identifying Android applications.
const char kAndroidAppScheme[] = "android";

// Returns a std::string_view corresponding to |component| in |uri|, or the empty
// string in case there is no such component.
std::string_view ComponentString(const std::string& uri,
                                 const url::Component& component) {
  if (!component.is_valid()) {
    return std::string_view();
  }
  return std::string_view(uri).substr(component.begin, component.len);
}

// Returns true if the passed ASCII |input| string contains nothing else than
// alphanumeric characters and those in |other_characters|.
bool ContainsOnlyAlphanumericAnd(std::string_view input,
                                 std::string_view other_characters) {
  for (char c : input) {
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) &&
        other_characters.find(c) == std::string_view::npos)
      return false;
  }
  return true;
}

// Canonicalizes a Web facet URI, and returns true if canonicalization was
// successful and produced a valid URI.
bool CanonicalizeWebFacetURI(const std::string& input_uri,
                             const url::Parsed& input_parsed,
                             std::string* canonical_uri) {
  url::Parsed canonical_parsed;
  url::StdStringCanonOutput canonical_output(canonical_uri);

  bool canonicalization_succeeded = url::CanonicalizeStandardURL(
      input_uri.c_str(), input_parsed,
      url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION, nullptr,
      &canonical_output, &canonical_parsed);
  canonical_output.Complete();

  if (canonicalization_succeeded && canonical_parsed.host.is_nonempty() &&
      !canonical_parsed.username.is_valid() &&
      !canonical_parsed.password.is_valid() &&
      ComponentString(*canonical_uri, canonical_parsed.path) == "/" &&
      !canonical_parsed.query.is_valid() && !canonical_parsed.ref.is_valid()) {
    // Get rid of the trailing slash added by url::CanonicalizeStandardURL().
    DCHECK_EQ((size_t)canonical_parsed.path.begin, canonical_uri->size() - 1);
    canonical_uri->erase(canonical_parsed.path.begin,
                         canonical_parsed.path.len);
    return true;
  }
  return false;
}

// Adds padding until the length of the base64-encoded |data| becomes a multiple
// of 4, and returns true if the thusly obtained |data| is now correctly padded,
// i.e., there are at most 2 padding characters ('=') at the very end.
bool CanonicalizeBase64Padding(std::string* data) {
  while (data->size() % 4u != 0u)
    data->push_back('=');

  size_t first_padding = data->find_first_of('=');
  return first_padding == std::string::npos ||
         (data->size() - first_padding <= 2u &&
          data->find_first_not_of('=', first_padding) == std::string::npos);
}

// Canonicalizes the username component in an Android facet URI (containing the
// certificate hash), and returns true if canonicalization was successful and
// produced a valid non-empty component.
bool CanonicalizeHashComponent(std::string_view input_hash,
                               url::CanonOutput* canonical_output) {
  // Characters other than alphanumeric that are used in the "URL and filename
  // safe" base64 alphabet; plus the padding ('=').
  const char kBase64NonAlphanumericChars[] = "-_=";

  std::string base64_encoded_hash =
      base::UnescapeBinaryURLComponent(input_hash);

  if (!base64_encoded_hash.empty() &&
      CanonicalizeBase64Padding(&base64_encoded_hash) &&
      ContainsOnlyAlphanumericAnd(base64_encoded_hash,
                                  kBase64NonAlphanumericChars)) {
    canonical_output->Append(base64_encoded_hash);
    canonical_output->push_back('@');
    return true;
  }
  return false;
}

// Canonicalizes the host component in an Android facet URI (containing the
// package name), and returns true if canonicalization was successful and
// produced a valid non-empty component.
bool CanonicalizePackageNameComponent(
    std::string_view input_package_name,
    url::CanonOutput* canonical_output) {
  // Characters other than alphanumeric that are permitted in the package names.
  const char kPackageNameNonAlphanumericChars[] = "._";

  std::string package_name =
      base::UnescapeBinaryURLComponent(input_package_name);

  // TODO(engedy): We might want to use a regex to check this more throughly.
  if (!package_name.empty() &&
      ContainsOnlyAlphanumericAnd(package_name,
                                  kPackageNameNonAlphanumericChars)) {
    canonical_output->Append(package_name);
    return true;
  }
  return false;
}

// Canonicalizes an Android facet URI, and returns true if canonicalization was
// successful and produced a valid URI.
bool CanonicalizeAndroidFacetURI(const std::string& input_uri,
                                 const url::Parsed& input_parsed,
                                 std::string* canonical_uri) {
  url::StdStringCanonOutput canonical_output(canonical_uri);

  url::Component unused;
  bool success = url::CanonicalizeScheme(
      input_uri.c_str(), input_parsed.scheme, &canonical_output, &unused);

  canonical_output.push_back('/');
  canonical_output.push_back('/');

  // We cannot use url::CanonicalizeUserInfo as that would percent encode the
  // base64 padding characters ('=').
  success &= CanonicalizeHashComponent(
      ComponentString(input_uri, input_parsed.username), &canonical_output);

  // We cannot use url::CanonicalizeHost as that would convert the package name
  // to lower case, but the package name is case sensitive.
  success &= CanonicalizePackageNameComponent(
      ComponentString(input_uri, input_parsed.host), &canonical_output);

  canonical_output.Complete();

  return success && input_parsed.password.is_empty() &&
         (input_parsed.path.is_empty() ||
          ComponentString(input_uri, input_parsed.path) == "/") &&
         input_parsed.port.is_empty() && !input_parsed.query.is_valid() &&
         !input_parsed.ref.is_valid();
}

// Computes the canonicalized form of |uri| into |canonical_uri|, and returns
// true if canonicalization was successful and produced a valid URI.
bool ParseAndCanonicalizeFacetURI(const std::string& input_uri,
                                  std::string* canonical_uri) {
  DCHECK(canonical_uri);
  canonical_uri->clear();
  canonical_uri->reserve(input_uri.size() + 32);

  url::Parsed input_parsed = url::ParseStandardURL(input_uri);
  std::string_view scheme = ComponentString(input_uri, input_parsed.scheme);
  if (base::EqualsCaseInsensitiveASCII(scheme, url::kHttpsScheme)) {
    return CanonicalizeWebFacetURI(input_uri, input_parsed, canonical_uri);
  }
  if (base::EqualsCaseInsensitiveASCII(scheme, kAndroidAppScheme)) {
    return CanonicalizeAndroidFacetURI(input_uri, input_parsed, canonical_uri);
  }

  *canonical_uri = input_uri;
  return false;
}

// Extracts and sorts the facet URIs of the given affiliated facets. This is
// used to determine whether two equivalence classes are equal.
std::vector<FacetURI> ExtractAndSortFacetURIs(const AffiliatedFacets& facets) {
  std::vector<FacetURI> uris;
  uris.reserve(facets.size());
  base::ranges::transform(facets, std::back_inserter(uris), &Facet::uri);
  std::sort(uris.begin(), uris.end());
  return uris;
}

// Appends a new level to the |main_domain| from |full_domain|.
// |main_domain| must be a suffix of |full_domain|.
void IncreaseDomainLevel(const std::string& full_domain,
                         std::string& main_domain) {
  DCHECK_GT(full_domain.size(), main_domain.size());
  auto starting_pos = full_domain.rbegin() + main_domain.size();
  // Verify that we are at '.' and move to the next character.
  DCHECK_EQ(*starting_pos, '.');
  starting_pos++;
  // Find next '.' from |starting_pos|
  auto ending_pos = std::find(starting_pos, full_domain.rend(), '.');
  main_domain = std::string(ending_pos.base(), full_domain.end());
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
    std::erase_if(disjoint_sets, [](const auto& set) { return set.empty(); });
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

}  // namespace


// FacetURI -------------------------------------------------------------------

FacetURI::FacetURI() = default;

// static
FacetURI FacetURI::FromPotentiallyInvalidSpec(const std::string& spec) {
  std::string canonical_spec;
  bool is_valid = ParseAndCanonicalizeFacetURI(spec, &canonical_spec);
  return FacetURI(canonical_spec, is_valid);
}

// static
FacetURI FacetURI::FromCanonicalSpec(const std::string& canonical_spec) {
  return FacetURI(canonical_spec, true);
}

bool FacetURI::IsValidWebFacetURI() const {
  return scheme() == url::kHttpsScheme;
}

bool FacetURI::IsValidAndroidFacetURI() const {
  return scheme() == kAndroidAppScheme;
}

std::string FacetURI::scheme() const {
  return is_valid()
             ? std::string(ComponentString(canonical_spec_, parsed_.scheme))
             : "";
}

std::string FacetURI::android_package_name() const {
  if (!IsValidAndroidFacetURI())
    return "";
  return std::string(ComponentString(canonical_spec_, parsed_.host));
}

std::string FacetURI::GetAndroidPackageDisplayName() const {
  CHECK(IsValidAndroidFacetURI());
  std::vector<std::string> parts = base::SplitString(
      android_package_name(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(parts.begin(), parts.end());
  return base::JoinString(parts, ".");
}

FacetURI::FacetURI(const std::string& canonical_spec, bool is_valid)
    : is_valid_(is_valid), canonical_spec_(canonical_spec) {
  // TODO(engedy): Refactor code in order to avoid to avoid parsing the URL
  // twice.
  parsed_ = url::ParseStandardURL(canonical_spec_);
}

// Facet

Facet::Facet(FacetURI uri,
             FacetBrandingInfo branding_info,
             GURL change_password_url,
             std::string main_domain)
    : uri(std::move(uri)),
      branding_info(std::move(branding_info)),
      change_password_url(std::move(change_password_url)),
      main_domain(std::move(main_domain)) {}

Facet::~Facet() = default;

Facet::Facet(const Facet& other) = default;

Facet::Facet(Facet&& other) = default;

Facet& Facet::operator=(const Facet& other) = default;

Facet& Facet::operator=(Facet&& other) = default;

// GroupedFacets

GroupedFacets::GroupedFacets() = default;

GroupedFacets::~GroupedFacets() = default;

GroupedFacets::GroupedFacets(const GroupedFacets& other) = default;

GroupedFacets::GroupedFacets(GroupedFacets&& other) = default;

GroupedFacets& GroupedFacets::operator=(const GroupedFacets& other) = default;

GroupedFacets& GroupedFacets::operator=(GroupedFacets&& other) = default;

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
              ? GetExtendedTopLevelDomain(
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

// AffiliatedFacetsWithUpdateTime ---------------------------------------------

AffiliatedFacetsWithUpdateTime::AffiliatedFacetsWithUpdateTime() = default;

AffiliatedFacetsWithUpdateTime::AffiliatedFacetsWithUpdateTime(
    const AffiliatedFacetsWithUpdateTime& other) = default;

AffiliatedFacetsWithUpdateTime::AffiliatedFacetsWithUpdateTime(
    AffiliatedFacetsWithUpdateTime&& other) = default;

AffiliatedFacetsWithUpdateTime& AffiliatedFacetsWithUpdateTime::operator=(
    const AffiliatedFacetsWithUpdateTime& other) = default;

AffiliatedFacetsWithUpdateTime& AffiliatedFacetsWithUpdateTime::operator=(
    AffiliatedFacetsWithUpdateTime&& other) = default;

AffiliatedFacetsWithUpdateTime::~AffiliatedFacetsWithUpdateTime() = default;

// Helpers --------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const FacetURI& facet_uri) {
  return os << facet_uri.potentially_invalid_spec();
}

bool operator==(const FacetBrandingInfo& lhs, const FacetBrandingInfo& rhs) {
  return std::tie(lhs.name, lhs.icon_url) == std::tie(rhs.name, rhs.icon_url);
}

bool operator!=(const FacetBrandingInfo& lhs, const FacetBrandingInfo& rhs) {
  return !(lhs == rhs);
}

bool operator==(const Facet& lhs, const Facet& rhs) {
  return std::tie(lhs.uri, lhs.branding_info, lhs.main_domain) ==
         std::tie(rhs.uri, rhs.branding_info, rhs.main_domain);
}

bool operator!=(const Facet& lhs, const Facet& rhs) {
  return !(lhs == rhs);
}

bool operator==(const GroupedFacets& lhs, const GroupedFacets& rhs) {
  if (!base::ranges::is_permutation(lhs.facets, rhs.facets)) {
    return false;
  }
  return lhs.branding_info == rhs.branding_info;
}

bool operator!=(const GroupedFacets& lhs, const GroupedFacets& rhs) {
  return !(lhs == rhs);
}

bool AreEquivalenceClassesEqual(const AffiliatedFacets& a,
                                const AffiliatedFacets& b) {
  return a.size() == b.size() &&
         ExtractAndSortFacetURIs(a) == ExtractAndSortFacetURIs(b);
}

bool IsValidAndroidFacetURI(const std::string& url) {
  FacetURI facet = FacetURI::FromPotentiallyInvalidSpec(url);
  return facet.IsValidAndroidFacetURI();
}

std::string GetExtendedTopLevelDomain(
    const GURL& url,
    const base::flat_set<std::string>& psl_extensions) {
  std::string main_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (main_domain.empty()) {
    return main_domain;
  }

  std::string full_domain = url.host();

  // Something went wrong, and it shouldn't happen. Return early in this case to
  // avoid undefined behaviour.
  if (!base::EndsWith(full_domain, main_domain)) {
    return main_domain;
  }

  // If a domain is contained within the PSL extension list, an additional
  // subdomain is added to that domain. This is done until the domain is not
  // contained within the PSL extension list or fully shown. For multi-level
  // extension, this approach only works if all sublevels are included in the
  // PSL extension list.
  while (main_domain != full_domain && psl_extensions.contains(main_domain)) {
    IncreaseDomainLevel(full_domain, main_domain);
  }
  return main_domain;
}

bool IsExtendedPublicSuffixDomainMatch(
    const GURL& url1,
    const GURL& url2,
    const base::flat_set<std::string>& psl_extensions) {
  if (!url1.is_valid() || !url2.is_valid()) {
    return false;
  }

  std::string domain1(GetExtendedTopLevelDomain(url1, psl_extensions));
  std::string domain2(GetExtendedTopLevelDomain(url2, psl_extensions));
  if (domain1.empty() || domain2.empty()) {
    return false;
  }

  return domain1 == domain2;
}

}  // namespace affiliations
