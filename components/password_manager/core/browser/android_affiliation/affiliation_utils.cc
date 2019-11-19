// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

#include <algorithm>
#include <ostream>

#include "base/base64.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/escape.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon_stdstring.h"

namespace password_manager {

namespace {

// The scheme used for identifying Android applications.
const char kAndroidAppScheme[] = "android";

// Returns a StringPiece corresponding to |component| in |uri|, or the empty
// string in case there is no such component.
base::StringPiece ComponentString(const std::string& uri,
                                  const url::Component& component) {
  if (!component.is_valid())
    return base::StringPiece();
  return base::StringPiece(uri.c_str() + component.begin, component.len);
}

// Returns true if the passed ASCII |input| string contains nothing else than
// alphanumeric characters and those in |other_characters|.
bool ContainsOnlyAlphanumericAnd(const base::StringPiece& input,
                                 const base::StringPiece& other_characters) {
  for (char c : input) {
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) &&
        other_characters.find(c) == base::StringPiece::npos)
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
      input_uri.c_str(), input_uri.size(), input_parsed,
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
bool CanonicalizeHashComponent(const base::StringPiece& input_hash,
                               url::CanonOutput* canonical_output) {
  // Characters other than alphanumeric that are used in the "URL and filename
  // safe" base64 alphabet; plus the padding ('=').
  const char kBase64NonAlphanumericChars[] = "-_=";

  std::string base64_encoded_hash = net::UnescapeBinaryURLComponent(input_hash);

  if (!base64_encoded_hash.empty() &&
      CanonicalizeBase64Padding(&base64_encoded_hash) &&
      ContainsOnlyAlphanumericAnd(base64_encoded_hash,
                                  kBase64NonAlphanumericChars)) {
    canonical_output->Append(base64_encoded_hash.data(),
                             base64_encoded_hash.size());
    canonical_output->push_back('@');
    return true;
  }
  return false;
}

// Canonicalizes the host component in an Android facet URI (containing the
// package name), and returns true if canonicalization was successful and
// produced a valid non-empty component.
bool CanonicalizePackageNameComponent(
    const base::StringPiece& input_package_name,
    url::CanonOutput* canonical_output) {
  // Characters other than alphanumeric that are permitted in the package names.
  const char kPackageNameNonAlphanumericChars[] = "._";

  std::string package_name =
      net::UnescapeBinaryURLComponent(input_package_name);

  // TODO(engedy): We might want to use a regex to check this more throughly.
  if (!package_name.empty() &&
      ContainsOnlyAlphanumericAnd(package_name,
                                  kPackageNameNonAlphanumericChars)) {
    canonical_output->Append(package_name.data(), package_name.size());
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

  return success && !input_parsed.password.is_nonempty() &&
         (!input_parsed.path.is_nonempty() ||
          ComponentString(input_uri, input_parsed.path) == "/") &&
         !input_parsed.port.is_nonempty() && !input_parsed.query.is_valid() &&
         !input_parsed.ref.is_valid();
}

// Computes the canonicalized form of |uri| into |canonical_uri|, and returns
// true if canonicalization was successful and produced a valid URI.
bool ParseAndCanonicalizeFacetURI(const std::string& input_uri,
                                  std::string* canonical_uri) {
  DCHECK(canonical_uri);
  canonical_uri->clear();
  canonical_uri->reserve(input_uri.size() + 32);

  url::Parsed input_parsed;
  url::ParseStandardURL(input_uri.c_str(), input_uri.size(), &input_parsed);

  base::StringPiece scheme = ComponentString(input_uri, input_parsed.scheme);
  if (base::LowerCaseEqualsASCII(scheme, url::kHttpsScheme)) {
    return CanonicalizeWebFacetURI(input_uri, input_parsed, canonical_uri);
  } else if (base::LowerCaseEqualsASCII(scheme, kAndroidAppScheme)) {
    return CanonicalizeAndroidFacetURI(input_uri, input_parsed, canonical_uri);
  }
  return false;
}

// Extracts and sorts the facet URIs of the given affiliated facets. This is
// used to determine whether two equivalence classes are equal.
std::vector<FacetURI> ExtractAndSortFacetURIs(const AffiliatedFacets& facets) {
  std::vector<FacetURI> uris;
  uris.reserve(facets.size());
  std::transform(facets.begin(), facets.end(), std::back_inserter(uris),
                 [](const Facet& facet) { return facet.uri; });
  std::sort(uris.begin(), uris.end());
  return uris;
}

}  // namespace


// FacetURI -------------------------------------------------------------------

FacetURI::FacetURI() : is_valid_(false) {
}

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

bool FacetURI::operator==(const FacetURI& other) const {
  DCHECK(is_empty() || is_valid());
  DCHECK(other.is_empty() || other.is_valid());
  return canonical_spec_ == other.canonical_spec_;
}

bool FacetURI::operator!=(const FacetURI& other) const {
  DCHECK(is_empty() || is_valid());
  DCHECK(other.is_empty() || other.is_valid());
  return canonical_spec_ != other.canonical_spec_;
}

bool FacetURI::operator<(const FacetURI& other) const {
  DCHECK(is_empty() || is_valid());
  DCHECK(other.is_empty() || other.is_valid());
  return canonical_spec_ < other.canonical_spec_;
}

bool FacetURI::operator>(const FacetURI& other) const {
  DCHECK(is_empty() || is_valid());
  DCHECK(other.is_empty() || other.is_valid());
  return canonical_spec_ > other.canonical_spec_;
}

bool FacetURI::IsValidWebFacetURI() const {
  return scheme() == url::kHttpsScheme;
}

bool FacetURI::IsValidAndroidFacetURI() const {
  return scheme() == kAndroidAppScheme;
}

std::string FacetURI::scheme() const {
  return is_valid()
             ? ComponentString(canonical_spec_, parsed_.scheme).as_string()
             : "";
}

std::string FacetURI::android_package_name() const {
  if (!IsValidAndroidFacetURI())
    return "";
  return ComponentString(canonical_spec_, parsed_.host).as_string();
}

FacetURI::FacetURI(const std::string& canonical_spec, bool is_valid)
    : is_valid_(is_valid), canonical_spec_(canonical_spec) {
  // TODO(engedy): Refactor code in order to avoid to avoid parsing the URL
  // twice.
  url::ParseStandardURL(canonical_spec_.c_str(), canonical_spec_.size(),
                        &parsed_);
}


// AffiliatedFacetsWithUpdateTime ---------------------------------------------

AffiliatedFacetsWithUpdateTime::AffiliatedFacetsWithUpdateTime() {
}

AffiliatedFacetsWithUpdateTime::AffiliatedFacetsWithUpdateTime(
    const AffiliatedFacetsWithUpdateTime& other) = default;

AffiliatedFacetsWithUpdateTime::~AffiliatedFacetsWithUpdateTime() {
}


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
  return std::tie(lhs.uri, lhs.branding_info) ==
         std::tie(rhs.uri, rhs.branding_info);
}

bool operator!=(const Facet& lhs, const Facet& rhs) {
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

}  // namespace password_manager
