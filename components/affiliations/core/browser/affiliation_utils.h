// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to working with "facets".
//
// A "facet" is defined as the manifestation of a logical application on a given
// platform. For example, "My Bank" may have released an Android application
// and a Web application accessible from a browser. These are all facets of the
// "My Bank" logical application.
//
// Facets that belong to the same logical application are said to be affiliated
// with each other. Conceptually, "affiliations" can be seen as an equivalence
// relation defined over the set of all facets. Each equivalence class contains
// facets that belong to the same logical application, and therefore should be
// treated as synonymous for certain purposes, e.g., sharing credentials.
//
// A valid facet identifier will be a URI of the form:
//
//   * https://<host>[:<port>]
//
//      For web sites. Only HTTPS sites are supported. The syntax corresponds to
//      that of 'serialized-origin' in RFC 6454. That is, in canonical form, the
//      URI must not contain components other than the scheme (required, must be
//      "https"), host (required), and port (optional); with canonicalization
//      performed the same way as it normally would be for standard URLs.
//
//   * android://<certificate_hash>@<package_name>
//
//      For Android applications. In canonical form, the URI must not contain
//      components other than the scheme (must be "android"), username, and host
//      (all required). The host part must be a valid Android package name, with
//      no escaping, so it must be composed of characters [a-zA-Z0-9_.].
//
//      The username part must be the hash of the certificate used to sign the
//      APK, base64-encoded using padding and the "URL and filename safe" base64
//      alphabet, with no further escaping. This is normally calculated as:
//
//        echo -n -e "$PEM_KEY" |
//          openssl x509 -outform DER |
//          openssl sha -sha512 -binary | base64 | tr '+/' '-_'
//

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_UTILS_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_UTILS_H_

#include <compare>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace affiliations {

// Encapsulates a facet URI in canonical form.
//
// This is a very light-weight wrapper around an std::string containing the text
// of the URI, and can be passed around as a value. The main rationale for the
// existence of this class is to make it clearer in the code when a certain URI
// is known to be a valid facet URI in canonical form, and to allow verifying
// and converting URIs to such canonical form.
//
// Note that it would be impractical to use GURL to represent facet URIs, as
// GURL has built-in logic to parse the rest of the URI according to its scheme,
// and obviously, it does not recognize the "android" scheme. Therefore, after
// parsing, everything ends up in the path component, which is not too helpful.
class FacetURI {
 public:
  // Constructs an instance to encapsulate the canonical form of |spec|.
  // If |spec| is not a valid facet URI, then an invalid instance is returned,
  // which then should be discarded.
  static FacetURI FromPotentiallyInvalidSpec(const std::string& spec);

  // Constructs a valid FacetURI instance from a valid |canonical_spec|.
  // Note: The passed-in URI is not verified at all. Use only when you are sure
  // the URI is valid and in canonical form.
  static FacetURI FromCanonicalSpec(const std::string& canonical_spec);

  FacetURI();

  // As a light-weight std::string wrapper, allow copy and assign.
  FacetURI(const FacetURI&) = default;
  FacetURI(FacetURI&&) = default;
  FacetURI& operator=(const FacetURI&) = default;
  FacetURI& operator=(FacetURI&&) = default;

  friend std::weak_ordering operator<=>(const FacetURI& lhs,
                                        const FacetURI& rhs) {
    return lhs.canonical_spec_ <=> rhs.canonical_spec_;
  }

  friend bool operator==(const FacetURI& lhs, const FacetURI& rhs) {
    return lhs.canonical_spec_ == rhs.canonical_spec_;
  }

  // Returns whether or not this instance represents a valid facet identifier
  // referring to a Web application.
  bool IsValidWebFacetURI() const;

  // Returns whether or not this instance represents a valid facet identifier
  // referring to an Android application.
  bool IsValidAndroidFacetURI() const;

  // Returns android_package_name() which can be displayed in the UI.
  std::string GetAndroidPackageDisplayName() const;

  // Returns whether or not this instance represents a valid facet identifier
  // referring to either a Web or an Android application. The empty identfier is
  // not considered valid.
  bool is_valid() const { return is_valid_; }

  // Returns whether or not this instance represents the empty facet identifier.
  bool is_empty() const { return canonical_spec_.empty(); }

  // Returns the canonical scheme of the encapsulated facet URI, provided it is
  // valid, or the empty string otherwise.
  std::string scheme() const;

  // Returns the canonical package name that the encapsulated facet URI
  // references, provided it is a valid Android facet URI, or the empty string
  // otherwise.
  std::string android_package_name() const;

  // Returns the text of the encapsulated canonical URI, which must be valid.
  const std::string& canonical_spec() const {
    DCHECK(is_valid_);
    return canonical_spec_;
  }

  // Returns the text of the encapsulated canonical URI, even if it is invalid.
  const std::string& potentially_invalid_spec() const {
    return canonical_spec_;
  }

 private:
  // Internal constructor to be used by the static factory methods.
  FacetURI(const std::string& canonical_spec, bool is_valid);

  // Whether |canonical_spec_| contains a valid facet URI in canonical form.
  bool is_valid_ = false;

  // The text of the encapsulated canonical URI, valid if and only if
  // |is_valid_| is true.
  std::string canonical_spec_;

  // Identified components of the canonical spec.
  url::Parsed parsed_;
};

// The branding information for a given facet. Corresponds to the |BrandingInfo|
// message in affiliation_api.proto.
struct FacetBrandingInfo {
  // Human readable name of this facet, or empty if this information is not
  // available.
  //
  // For example, this would be something like "Netflix" for the popular
  // video-on-demand application corresponding to FacetURIs
  // `android://...@com.netflix.mediaclient` and `https://netflix.com`.
  std::string name;

  // URL of the icon of this facet, or empty if this information is not
  // available.
  //
  // For example, this would be something like
  // "https://lh3.googleusercontent.com/..." for the popular video-on-demand
  // application corresponding to FacetURIs
  // `android://...@com.netflix.mediaclient` and `https://netflix.com`.
  GURL icon_url;
};

// Facet struct, corresponds to the |Facet| message in affiliation_api.proto.
struct Facet {
  explicit Facet(FacetURI uri,
                 FacetBrandingInfo branding_info = FacetBrandingInfo(),
                 GURL change_password_url = GURL(),
                 std::string main_domain = std::string());

  ~Facet();

  Facet(const Facet& other);
  Facet(Facet&& other);
  Facet& operator=(const Facet& other);
  Facet& operator=(Facet&& other);

  FacetURI uri;
  FacetBrandingInfo branding_info;
  GURL change_password_url;
  std::string main_domain;
};

// A collection of facets affiliated with each other, i.e. an equivalence class.
using AffiliatedFacets = std::vector<Facet>;

// A collection of grouped facets. Used to group passwords in the UI.
struct GroupedFacets {
  GroupedFacets();
  ~GroupedFacets();
  GroupedFacets(const GroupedFacets& other);
  GroupedFacets(GroupedFacets&& other);
  GroupedFacets& operator=(const GroupedFacets& other);
  GroupedFacets& operator=(GroupedFacets&& other);

  // Facets which are representing a group.
  std::vector<Facet> facets;

  // Group branding info.
  FacetBrandingInfo branding_info;
};

// This functions merges groups together if one of the following applies:
// * the same facet is present in both groups.
// * eTLD+1 of a facet in one group matches eTLD+1 of a facet in another group.
std::vector<GroupedFacets> MergeRelatedGroups(
    const base::flat_set<std::string>& psl_extensions,
    const std::vector<GroupedFacets>& groups);

// A collection of facets affiliated with each other, i.e. an equivalence class,
// plus a timestamp that indicates the last time the data was updated from an
// authoritative source.
struct AffiliatedFacetsWithUpdateTime {
  AffiliatedFacetsWithUpdateTime();
  AffiliatedFacetsWithUpdateTime(const AffiliatedFacetsWithUpdateTime& other);
  AffiliatedFacetsWithUpdateTime(AffiliatedFacetsWithUpdateTime&& other);
  AffiliatedFacetsWithUpdateTime& operator=(
      const AffiliatedFacetsWithUpdateTime& other);
  AffiliatedFacetsWithUpdateTime& operator=(
      AffiliatedFacetsWithUpdateTime&& other);
  ~AffiliatedFacetsWithUpdateTime();

  AffiliatedFacets facets;
  base::Time last_update_time;
};

// Returns whether or not equivalence classes |a| and |b| are equal, that is,
// whether or not they consist of the same set of facet URIs. Note that branding
// information is ignored for this check.
//
// Note that this will do some sorting, so it can be expensive for large inputs.
bool AreEquivalenceClassesEqual(const AffiliatedFacets& a,
                                const AffiliatedFacets& b);

// A shorter way to spell FacetURI::IsValidAndroidFacetURI().
bool IsValidAndroidFacetURI(const std::string& uri);

// Retrieves the extended top level domain for a given |url|
// ("https://www.facebook.com/" => "facebook.com"). If the calculated top
// private domain matches an entry from the |psl_extensions| (e.g. "app.link"),
// the domain is extended by one level ("https://facebook.app.link/" =>
// "facebook.app.link"). If the |url| is not a valid URI or has an unsupported
// schema (e.g. "android://"), empty string is returned.
std::string GetExtendedTopLevelDomain(
    const GURL& url,
    const base::flat_set<std::string>& psl_extensions);

// Two URLs are considered an Extended Public Suffix Domain match if they have
// the same extended top level domain (See `GetExtendedTopLevelDomain`). The
// `psl_extensions` list is used to calculate the appropriate top domain. If one
// or both arguments do not describe valid URLs, returns false.
bool IsExtendedPublicSuffixDomainMatch(
    const GURL& url1,
    const GURL& url2,
    const base::flat_set<std::string>& psl_extensions);

// For logging use only.
std::ostream& operator<<(std::ostream& os, const FacetURI& facet_uri);

// Needed for testing.
bool operator==(const FacetBrandingInfo& lhs, const FacetBrandingInfo& rhs);
bool operator!=(const FacetBrandingInfo& lhs, const FacetBrandingInfo& rhs);
bool operator==(const Facet& lhs, const Facet& rhs);
bool operator!=(const Facet& lhs, const Facet& rhs);
bool operator==(const GroupedFacets& lhs, const GroupedFacets& rhs);
bool operator!=(const GroupedFacets& lhs, const GroupedFacets& rhs);

struct FacetURIHash {
  size_t operator()(const FacetURI& facet_uri) const {
    return std::hash<std::string>()(facet_uri.potentially_invalid_spec());
  }
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_UTILS_H_
