// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/content_settings/core/common/content_settings_pattern.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Array of non domain wildcard and non-port scheme names, and their count.
const char* const* g_non_domain_wildcard_non_port_schemes = nullptr;
size_t g_non_domain_wildcard_non_port_schemes_count = 0;

// Keep it consistent with enum SchemeType in content_settings_pattern.h.
// TODO(msramek): Layering violation: assemble this array from hardcoded
// schemes and those injected via |SetNonWildcardDomainNonPortSchemes()|.
const char* const kSchemeNames[] = {"wildcard",         "other",
                                    url::kHttpScheme,   url::kHttpsScheme,
                                    url::kFileScheme,   "chrome-extension",
                                    "chrome-search",    "chrome",
                                    "chrome-untrusted", "devtools",
                                    "isolated-app"};

static_assert(std::size(kSchemeNames) == ContentSettingsPattern::SCHEME_MAX,
              "kSchemeNames should have SCHEME_MAX elements");

// Note: it is safe to return a std::string_view here as long as they are
// either empty or referencing constant string literals.
std::string_view GetDefaultPort(std::string_view scheme) {
  if (scheme == url::kHttpScheme)
    return "80";
  if (scheme == url::kHttpsScheme)
    return "443";
  return std::string_view();
}

// Returns true if |sub_domain| is a sub domain or equals |domain|.  E.g.
// "mail.google.com" is a sub domain of "google.com" but "evilhost.com" is not a
// subdomain of "host.com".
bool IsSubDomainOrEqual(std::string_view sub_domain, std::string_view domain) {
  // The empty string serves as wildcard. Each domain is a subdomain of the
  // wildcard.
  if (domain.empty())
    return true;

  // The two domains are identical.
  if (domain == sub_domain)
    return true;

  // The |domain| is a proper domain-suffix of the |sub_domain|.
  return sub_domain.length() > domain.length() &&
         sub_domain[sub_domain.length() - domain.length() - 1] == '.' &&
         base::EndsWith(sub_domain, domain, base::CompareCase::SENSITIVE);
}

// Splits a |domain| name on the last dot. The returned tuple will consist of:
//  (1) A prefix of the |domain| name such that the right-most domain label and
//      its separating dot is removed; or std::nullopt if |domain| consisted
//      only of a single domain label.
//  (2) The right-most domain label, which is defined as the empty string if
//      |domain| is empty or ends in a dot.
std::tuple<std::optional<std::string_view>, std::string_view>
SplitDomainOnLastDot(std::string_view domain) {
  size_t index_of_last_dot = domain.rfind('.');
  if (index_of_last_dot == std::string_view::npos) {
    return std::make_tuple(std::nullopt, domain);
  }
  return std::make_tuple(domain.substr(0, index_of_last_dot),
                         domain.substr(index_of_last_dot + 1));
}

// Compares two domain names.
int CompareDomainNames(std::string_view domain_a, std::string_view domain_b) {
  std::optional<std::string_view> rest_of_a(domain_a);
  std::optional<std::string_view> rest_of_b(domain_b);

  while (rest_of_a && rest_of_b) {
    std::string_view rightmost_label_a;
    std::string_view rightmost_label_b;
    std::tie(rest_of_a, rightmost_label_a) = SplitDomainOnLastDot(*rest_of_a);
    std::tie(rest_of_b, rightmost_label_b) = SplitDomainOnLastDot(*rest_of_b);

    // Domain names are stored in puny code. So it's fine to use the compare
    // method.
    int rv = rightmost_label_a.compare(rightmost_label_b);
    if (rv != 0)
      return rv;
  }

  if (rest_of_a && !rest_of_b)
    return 1;

  if (!rest_of_a && rest_of_b)
    return -1;

  // The domain names are identical.
  DCHECK(!rest_of_a && !rest_of_b);
  return 0;
}

typedef ContentSettingsPattern::BuilderInterface BuilderInterface;

}  // namespace

// ////////////////////////////////////////////////////////////////////////////
// ContentSettingsPattern::Builder
//
class ContentSettingsPattern::Builder
    : public ContentSettingsPattern::BuilderInterface {
 public:
  Builder();

  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  ~Builder() override;

  // BuilderInterface:
  BuilderInterface* WithPort(const std::string& port) override;
  BuilderInterface* WithPortWildcard() override;
  BuilderInterface* WithHost(const std::string& host) override;
  BuilderInterface* WithDomainWildcard() override;
  BuilderInterface* WithScheme(const std::string& scheme) override;
  BuilderInterface* WithSchemeWildcard() override;
  BuilderInterface* WithPath(const std::string& path) override;
  BuilderInterface* WithPathWildcard() override;
  BuilderInterface* Invalid() override;
  ContentSettingsPattern Build() override;

 private:
  // Canonicalizes the pattern parts so that they are ASCII only, either
  // in original (if it was already ASCII) or punycode form. Returns true if
  // the canonicalization was successful.
  static bool Canonicalize(PatternParts* parts);

  // Returns true when the pattern |parts| represent a valid pattern.
  static bool Validate(const PatternParts& parts);

  bool is_valid_;

  PatternParts parts_;
};

ContentSettingsPattern::Builder::Builder() : is_valid_(true) {}

ContentSettingsPattern::Builder::~Builder() = default;

BuilderInterface* ContentSettingsPattern::Builder::WithPort(
    const std::string& port) {
  parts_.port = port;
  parts_.is_port_wildcard = false;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithPortWildcard() {
  parts_.port.clear();
  parts_.is_port_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithHost(
    const std::string& host) {
  parts_.host = host;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithDomainWildcard() {
  parts_.has_domain_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithScheme(
    const std::string& scheme) {
  parts_.scheme = scheme;
  parts_.is_scheme_wildcard = false;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithSchemeWildcard() {
  parts_.scheme.clear();
  parts_.is_scheme_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithPath(
    const std::string& path) {
  parts_.path = path;
  parts_.is_path_wildcard = false;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithPathWildcard() {
  parts_.path.clear();
  parts_.is_path_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::Invalid() {
  is_valid_ = false;
  return this;
}

ContentSettingsPattern ContentSettingsPattern::Builder::Build() {
  if (!is_valid_)
    return ContentSettingsPattern();
  if (!Canonicalize(&parts_))
    return ContentSettingsPattern();
  is_valid_ = Validate(parts_);
  if (!is_valid_)
    return ContentSettingsPattern();

#if !defined(NDEBUG)
  // For debug builds, check that canonicalization is idempotent.
  PatternParts twice_canonicalized_parts(parts_);
  DCHECK(Canonicalize(&twice_canonicalized_parts));
  DCHECK(ContentSettingsPattern(std::move(twice_canonicalized_parts), true) ==
         ContentSettingsPattern(parts_, true));
#endif

  return ContentSettingsPattern(std::move(parts_), true);
}

// static
bool ContentSettingsPattern::Builder::Canonicalize(PatternParts* parts) {
  // Canonicalize the scheme part.
  parts->scheme = base::ToLowerASCII(parts->scheme);

  if (parts->scheme == url::kFileScheme && !parts->is_path_wildcard) {
    // TODO(crbug.com/40150835): Remove this loop once GURL canonicalization is
    // idempotent (see crbug.com/1128999).
    while (true) {
      std::string url_spec = base::StrCat(
          {url::kFileScheme, url::kStandardSchemeSeparator, parts->path});
      GURL url(url_spec);
      if (!url.is_valid())
        return false;
      if (parts->path == url.path_piece())
        break;
      parts->path = url.path();
    }
  }

  // Canonicalize the host part.
  url::CanonHostInfo host_info;
  std::string canonicalized_host;
  if (parts->scheme == url::kFileScheme) {
    canonicalized_host = net::CanonicalizeFileHost(parts->host, &host_info);
  } else {
    canonicalized_host = net::CanonicalizeHost(parts->host, &host_info);
  }

  if (host_info.IsIPAddress() && parts->has_domain_wildcard)
    return false;

  // A domain wildcard pattern involves exactly one separating dot, inside the
  // square brackets. This is a common misunderstanding of that pattern that we
  // want to check for. See: https://crbug.com/823706.
  if (parts->has_domain_wildcard && base::StartsWith(canonicalized_host, "."))
    return false;

  // Omit a single ending dot as long as there is at least one non-dot character
  // before it, which is in line with the behavior of net::TrimEndingDot; but
  // consider two ending dots an invalid pattern, otherwise canonicalization of
  // a canonical pattern would not be idempotent.
  if (base::EndsWith(canonicalized_host, "..", base::CompareCase::SENSITIVE)) {
    return false;
  } else if (canonicalized_host.size() >= 2u &&
             base::EndsWith(canonicalized_host, ".",
                            base::CompareCase::SENSITIVE)) {
    canonicalized_host.pop_back();
  }

  if ((parts->host.find('*') == std::string::npos) &&
      !canonicalized_host.empty()) {
    // Valid host.
    parts->host = std::move(canonicalized_host);
  } else {
    parts->host.clear();
  }

  return true;
}

// static
bool ContentSettingsPattern::Builder::Validate(const PatternParts& parts) {
  // Sanity checks first: {scheme, port, path} wildcards imply
  // empty {scheme, port, path}.
  if ((parts.is_scheme_wildcard && !parts.scheme.empty()) ||
      (parts.is_port_wildcard && !parts.port.empty()) ||
      (parts.is_path_wildcard && !parts.path.empty())) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // file:// URL patterns have an empty host and port.
  if (parts.scheme == url::kFileScheme) {
    if (parts.has_domain_wildcard || !parts.host.empty() || !parts.port.empty())
      return false;
    if (parts.is_path_wildcard)
      return parts.path.empty();
    return (!parts.path.empty() && parts.path != "/" &&
            parts.path.find('*') == std::string::npos);
  }

  // If the pattern is for a URL with a non-wildcard domain without a port,
  // test if it is valid.
  if (IsNonWildcardDomainNonPortScheme(parts.scheme) && parts.port.empty() &&
      !parts.is_port_wildcard) {
    return true;
  }

  // Non-file patterns are invalid if either the scheme, host or port part is
  // empty.
  if ((parts.scheme.empty() && !parts.is_scheme_wildcard) ||
      (parts.host.empty() && !parts.has_domain_wildcard) ||
      (parts.port.empty() && !parts.is_port_wildcard)) {
    return false;
  }

  if (parts.host.find('*') != std::string::npos)
    return false;

  // Test if the scheme is supported or a wildcard.
  if (!parts.is_scheme_wildcard && parts.scheme != url::kHttpScheme &&
      parts.scheme != url::kHttpsScheme) {
    return false;
  }
  return true;
}

// ////////////////////////////////////////////////////////////////////////////
// ContentSettingsPattern::PatternParts
//
ContentSettingsPattern::PatternParts::PatternParts()
    : is_scheme_wildcard(false),
      has_domain_wildcard(false),
      is_port_wildcard(false),
      is_path_wildcard(false) {}

ContentSettingsPattern::PatternParts::PatternParts(const PatternParts& other) =
    default;
ContentSettingsPattern::PatternParts::PatternParts(PatternParts&& other) =
    default;

ContentSettingsPattern::PatternParts::~PatternParts() = default;

ContentSettingsPattern::PatternParts&
ContentSettingsPattern::PatternParts::operator=(const PatternParts& other) =
    default;
ContentSettingsPattern::PatternParts&
ContentSettingsPattern::PatternParts::operator=(PatternParts&& other) = default;

bool ContentSettingsPattern::PatternParts::operator==(
    const ContentSettingsPattern::PatternParts& other) const = default;

// ////////////////////////////////////////////////////////////////////////////
// ContentSettingsPattern
//

// The version of the pattern format implemented. Version 1 includes the
// following patterns:
//   - [*.]domain.tld (matches domain.tld and all sub-domains)
//   - host (matches an exact hostname)
//   - a.b.c.d (matches an exact IPv4 ip)
//   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
//   - file:///tmp/test.html (a complete URL without a host)
const int ContentSettingsPattern::kContentSettingsPatternVersion = 1;

// static
std::unique_ptr<BuilderInterface> ContentSettingsPattern::CreateBuilder() {
  return std::make_unique<Builder>();
}

// static
ContentSettingsPattern ContentSettingsPattern::Wildcard() {
  PatternParts parts;
  parts.is_scheme_wildcard = true;
  parts.has_domain_wildcard = true;
  parts.is_port_wildcard = true;
  parts.is_path_wildcard = true;
  return ContentSettingsPattern(parts, true);
}

// static
ContentSettingsPattern ContentSettingsPattern::FromURL(const GURL& url) {
  ContentSettingsPattern::Builder builder;
  const GURL* local_url = &url;
  if (url.SchemeIsFileSystem() && url.inner_url()) {
    local_url = url.inner_url();
  }
  if (local_url->SchemeIsFile()) {
    builder.WithScheme(local_url->scheme())->WithPath(local_url->path());
  } else {
    // Please keep the order of the ifs below as URLs with an IP as host can
    // also have a "http" scheme.
    const bool is_non_wildcard_portless_scheme =
        IsNonWildcardDomainNonPortScheme(local_url->scheme());
    if (local_url->HostIsIPAddress()) {
      builder.WithScheme(local_url->scheme())->WithHost(local_url->host());
    } else if (local_url->SchemeIs(url::kHttpScheme)) {
      builder.WithSchemeWildcard()->WithDomainWildcard()->WithHost(
          local_url->host());
    } else if (local_url->SchemeIs(url::kHttpsScheme)) {
      builder.WithScheme(local_url->scheme())
          ->WithDomainWildcard()
          ->WithHost(local_url->host());
    } else if (is_non_wildcard_portless_scheme) {
      builder.WithScheme(local_url->scheme())->WithHost(local_url->host());
    } else {
      // Unsupported scheme
    }
    if (local_url->port_piece().empty()) {
      if (local_url->SchemeIs(url::kHttpsScheme)) {
        builder.WithPort(std::string(GetDefaultPort(url::kHttpsScheme)));
      } else if (!is_non_wildcard_portless_scheme) {
        builder.WithPortWildcard();
      }
    } else {
      builder.WithPort(local_url->port());
    }
  }
  return builder.Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::FromURLNoWildcard(
    const GURL& url) {
  ContentSettingsPattern::Builder builder;
  const GURL* local_url = &url;
  if (url.SchemeIsFileSystem() && url.inner_url()) {
    local_url = url.inner_url();
  }
  if (local_url->SchemeIsFile()) {
    builder.WithScheme(local_url->scheme())->WithPath(local_url->path());
  } else {
    builder.WithScheme(local_url->scheme())->WithHost(local_url->host());
    if (local_url->port_piece().empty()) {
      builder.WithPort(std::string(GetDefaultPort(local_url->scheme_piece())));
    } else {
      builder.WithPort(local_url->port());
    }
  }
  return builder.Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::FromURLToSchemefulSitePattern(
    const GURL& url) {
  std::string registrable_domain = GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  auto builder = ContentSettingsPattern::CreateBuilder();

  if (registrable_domain.empty()) {
    registrable_domain = url.host();
  } else {
    builder->WithDomainWildcard();
  }

  return builder->WithScheme(url.scheme())
      ->WithHost(registrable_domain)
      ->WithPathWildcard()
      ->WithPortWildcard()
      ->Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::FromString(
    std::string_view pattern_spec) {
  ContentSettingsPattern::Builder builder;
  content_settings::PatternParser::Parse(pattern_spec, &builder);
  return builder.Build();
}

// static
void ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
    const char* const* schemes,
    size_t count) {
  DCHECK(schemes || count == 0);
  if (g_non_domain_wildcard_non_port_schemes) {
    DCHECK_EQ(g_non_domain_wildcard_non_port_schemes_count, count);
    for (size_t i = 0; i < count; ++i) {
      DCHECK_EQ(g_non_domain_wildcard_non_port_schemes[i], schemes[i]);
    }
  }

  g_non_domain_wildcard_non_port_schemes = schemes;
  g_non_domain_wildcard_non_port_schemes_count = count;
}

// static
bool ContentSettingsPattern::IsNonWildcardDomainNonPortScheme(
    std::string_view scheme) {
  DCHECK(g_non_domain_wildcard_non_port_schemes ||
         g_non_domain_wildcard_non_port_schemes_count == 0);
  for (size_t i = 0; i < g_non_domain_wildcard_non_port_schemes_count; ++i) {
    if (g_non_domain_wildcard_non_port_schemes[i] == scheme) {
      return true;
    }
  }
  return false;
}

// static
ContentSettingsPattern ContentSettingsPattern::ToDomainWildcardPattern(
    const ContentSettingsPattern& pattern) {
  DCHECK(pattern.IsValid());
  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          pattern.GetHost(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (registrable_domain.empty()) {
    return CreateBuilder()->Invalid()->Build();
  }

  return CreateBuilder()
      ->WithHost(registrable_domain)
      ->WithDomainWildcard()
      ->WithSchemeWildcard()
      ->WithPortWildcard()
      ->WithPathWildcard()
      ->Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::ToHostOnlyPattern(
    const ContentSettingsPattern& pattern) {
  DCHECK(pattern.IsValid());
  auto builder = CreateBuilder();
  builder->WithHost(pattern.GetHost());
  builder->WithSchemeWildcard();
  builder->WithPortWildcard();
  builder->WithPathWildcard();
  if (pattern.HasDomainWildcard()) {
    builder->WithDomainWildcard();
  }
  return builder->Build();
}

bool ContentSettingsPattern::CompareDomains::operator()(
    std::string_view domain_a,
    std::string_view domain_b) const {
  if (domain_a == domain_b) {
    return false;
  }

  if (net::IsSubdomainOf(domain_a, domain_b)) {
    return true;
  }
  if (net::IsSubdomainOf(domain_b, domain_a)) {
    return false;
  }
  return CompareDomainNames(domain_a, domain_b) < 0;
}

ContentSettingsPattern::ContentSettingsPattern() : is_valid_(false) {}

ContentSettingsPattern::ContentSettingsPattern(PatternParts parts, bool valid)
    : parts_(std::move(parts)), is_valid_(valid) {}

bool ContentSettingsPattern::Matches(const GURL& url) const {
  // An invalid pattern matches nothing.
  if (!is_valid_)
    return false;

  const GURL* local_url = &url;
  if (url.SchemeIsFileSystem() && url.inner_url()) {
    local_url = url.inner_url();
  }

  // Match the scheme part.
  if (!parts_.is_scheme_wildcard &&
      parts_.scheme != local_url->scheme_piece()) {
    return false;
  }

  // File URLs have no host. Matches if the pattern has the path wildcard set,
  // or if the path in the URL is identical to the one in the pattern.
  // For filesystem:file URLs, the path used is the filesystem type, so all
  // filesystem:file:///temporary/... are equivalent.
  // TODO(msramek): The file scheme should not behave differently when nested
  // inside the filesystem scheme. Investigate and fix.
  if (!parts_.is_scheme_wildcard &&
      local_url->scheme_piece() == url::kFileScheme)
    return parts_.is_path_wildcard || parts_.path == local_url->path_piece();

  // Match the host part. Code is the same as url::TrimEndingDot but that method
  // unnecessarily creates a new std::string.
  std::string_view trimmed_host = local_url->host_piece();
  size_t len = trimmed_host.length();
  if (len > 1 && trimmed_host[len - 1] == '.') {
    trimmed_host.remove_suffix(1);
  }

  if (!parts_.has_domain_wildcard) {
    if (parts_.host != trimmed_host)
      return false;
  } else {
    if (!IsSubDomainOrEqual(trimmed_host, parts_.host))
      return false;
  }

  // Ignore the port if the scheme doesn't support it.
  if (IsNonWildcardDomainNonPortScheme(parts_.scheme))
    return true;

  // Match the port part.
  // Use the default port if the port string is empty. GURL returns an empty
  // string if no port at all was specified or if the default port was
  // specified.
  const std::string_view port = local_url->port_piece().empty()
                                    ? GetDefaultPort(local_url->scheme_piece())
                                    : local_url->port_piece();
  if (!parts_.is_port_wildcard && parts_.port != port) {
    return false;
  }

  return true;
}

bool ContentSettingsPattern::MatchesAllHosts() const {
  return parts_.has_domain_wildcard && parts_.host.empty();
}

bool ContentSettingsPattern::MatchesSingleOrigin() const {
  return !parts_.is_scheme_wildcard && !parts_.has_domain_wildcard &&
         !parts_.is_port_wildcard && !parts_.is_path_wildcard;
}

bool ContentSettingsPattern::HasDomainWildcard() const {
  return parts_.has_domain_wildcard && !parts_.host.empty();
}

std::string ContentSettingsPattern::ToString() const {
  if (IsValid())
    return content_settings::PatternParser::ToString(parts_);
  return std::string();
}

GURL ContentSettingsPattern::ToRepresentativeUrl() const {
  if (IsValid()) {
    GURL url = content_settings::PatternParser::ToRepresentativeUrl(parts_);
    DCHECK(!url.is_valid() || Matches(url))
        << "Invalid conversion: " << ToString() << " to " << url;
    return url;
  }
  return GURL();
}

ContentSettingsPattern::SchemeType ContentSettingsPattern::GetScheme() const {
  if (parts_.is_scheme_wildcard)
    return SCHEME_WILDCARD;

  for (size_t i = 2; i < std::size(kSchemeNames); ++i) {
    if (parts_.scheme == kSchemeNames[i])
      return static_cast<SchemeType>(i);
  }
  return SCHEME_OTHER;
}

const std::string& ContentSettingsPattern::GetHost() const {
  return parts_.host;
}

ContentSettingsPattern::Relation ContentSettingsPattern::Compare(
    const ContentSettingsPattern& other) const {
  // Two invalid patterns are identical in the way they behave. They don't match
  // anything and are represented as an empty string. So it's fair to treat them
  // as identical.
  if ((this == &other) || (!is_valid_ && !other.is_valid_))
    return IDENTITY;

  if (!is_valid_ && other.is_valid_)
    return DISJOINT_ORDER_POST;
  if (is_valid_ && !other.is_valid_)
    return DISJOINT_ORDER_PRE;

  // If either host, port or scheme are disjoint return immediately.
  Relation host_relation = CompareHost(parts_, other.parts_);
  if (host_relation == DISJOINT_ORDER_PRE ||
      host_relation == DISJOINT_ORDER_POST)
    return host_relation;

  Relation port_relation = ComparePort(parts_, other.parts_);
  if (port_relation == DISJOINT_ORDER_PRE ||
      port_relation == DISJOINT_ORDER_POST)
    return port_relation;

  Relation scheme_relation = CompareScheme(parts_, other.parts_);
  if (scheme_relation == DISJOINT_ORDER_PRE ||
      scheme_relation == DISJOINT_ORDER_POST)
    return scheme_relation;

  Relation path_relation = ComparePath(parts_, other.parts_);
  if (path_relation == DISJOINT_ORDER_PRE ||
      path_relation == DISJOINT_ORDER_POST)
    return path_relation;

  if (host_relation != IDENTITY)
    return host_relation;
  if (port_relation != IDENTITY)
    return port_relation;
  if (scheme_relation != IDENTITY)
    return scheme_relation;
  return path_relation;
}

bool ContentSettingsPattern::operator==(
    const ContentSettingsPattern& other) const {
  return Compare(other) == IDENTITY;
}

bool ContentSettingsPattern::operator!=(
    const ContentSettingsPattern& other) const {
  return !(*this == other);
}

bool ContentSettingsPattern::operator<(
    const ContentSettingsPattern& other) const {
  return Compare(other) < 0;
}

bool ContentSettingsPattern::operator>(
    const ContentSettingsPattern& other) const {
  return Compare(other) > 0;
}

// static
ContentSettingsPattern::Relation ContentSettingsPattern::CompareScheme(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  if (parts.is_scheme_wildcard && !other_parts.is_scheme_wildcard)
    return ContentSettingsPattern::SUCCESSOR;
  if (!parts.is_scheme_wildcard && other_parts.is_scheme_wildcard)
    return ContentSettingsPattern::PREDECESSOR;

  int result = parts.scheme.compare(other_parts.scheme);
  if (result == 0)
    return ContentSettingsPattern::IDENTITY;
  if (result > 0)
    return ContentSettingsPattern::DISJOINT_ORDER_PRE;
  return ContentSettingsPattern::DISJOINT_ORDER_POST;
}

// static
ContentSettingsPattern::Relation ContentSettingsPattern::CompareHost(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  if (!parts.has_domain_wildcard && !other_parts.has_domain_wildcard) {
    // Case 1: No host starts with a wild card
    int result = CompareDomainNames(parts.host, other_parts.host);
    if (result == 0)
      return ContentSettingsPattern::IDENTITY;
    if (result < 0)
      return ContentSettingsPattern::DISJOINT_ORDER_PRE;
    return ContentSettingsPattern::DISJOINT_ORDER_POST;
  }
  if (parts.has_domain_wildcard && !other_parts.has_domain_wildcard) {
    // Case 2: |host| starts with a domain wildcard and |other_host| does not
    // start with a domain wildcard.
    // Examples:
    // "this" host:   [*.]google.com
    // "other" host:  google.com
    //
    // [*.]google.com
    // mail.google.com
    //
    // [*.]mail.google.com
    // google.com
    //
    // [*.]youtube.com
    // google.de
    //
    // [*.]youtube.com
    // mail.google.com
    //
    // *
    // google.de
    if (IsSubDomainOrEqual(other_parts.host, parts.host))
      return ContentSettingsPattern::SUCCESSOR;
    if (CompareDomainNames(parts.host, other_parts.host) < 0)
      return ContentSettingsPattern::DISJOINT_ORDER_PRE;
    return ContentSettingsPattern::DISJOINT_ORDER_POST;
  }
  if (!parts.has_domain_wildcard && other_parts.has_domain_wildcard) {
    // Case 3: |host| starts NOT with a domain wildcard and |other_host| starts
    // with a domain wildcard.
    if (IsSubDomainOrEqual(parts.host, other_parts.host))
      return ContentSettingsPattern::PREDECESSOR;
    if (CompareDomainNames(parts.host, other_parts.host) < 0)
      return ContentSettingsPattern::DISJOINT_ORDER_PRE;
    return ContentSettingsPattern::DISJOINT_ORDER_POST;
  }
  if (parts.has_domain_wildcard && other_parts.has_domain_wildcard) {
    // Case 4: |host| and |other_host| both start with a domain wildcard.
    // Examples:
    // [*.]google.com
    // [*.]google.com
    //
    // [*.]google.com
    // [*.]mail.google.com
    //
    // [*.]youtube.com
    // [*.]google.de
    //
    // [*.]youtube.com
    // [*.]mail.google.com
    //
    // [*.]youtube.com
    // *
    //
    // *
    // [*.]youtube.com
    if (parts.host == other_parts.host)
      return ContentSettingsPattern::IDENTITY;
    if (IsSubDomainOrEqual(other_parts.host, parts.host))
      return ContentSettingsPattern::SUCCESSOR;
    if (IsSubDomainOrEqual(parts.host, other_parts.host))
      return ContentSettingsPattern::PREDECESSOR;
    if (CompareDomainNames(parts.host, other_parts.host) < 0)
      return ContentSettingsPattern::DISJOINT_ORDER_PRE;
    return ContentSettingsPattern::DISJOINT_ORDER_POST;
  }

  NOTREACHED_IN_MIGRATION();
  return ContentSettingsPattern::IDENTITY;
}

// static
ContentSettingsPattern::Relation ContentSettingsPattern::ComparePort(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  if (parts.is_port_wildcard && !other_parts.is_port_wildcard)
    return ContentSettingsPattern::SUCCESSOR;
  if (!parts.is_port_wildcard && other_parts.is_port_wildcard)
    return ContentSettingsPattern::PREDECESSOR;

  int result = parts.port.compare(other_parts.port);
  if (result == 0)
    return ContentSettingsPattern::IDENTITY;
  if (result > 0)
    return ContentSettingsPattern::DISJOINT_ORDER_PRE;
  return ContentSettingsPattern::DISJOINT_ORDER_POST;
}

ContentSettingsPattern::Relation ContentSettingsPattern::ComparePath(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  // Path is only set (in builder methods) and checked (in |Matches()|) for
  // file:// URLs. For all other schemes, path is completely disregarded,
  // and thus the result of this comparison is identity.
  if (parts.scheme != url::kFileScheme ||
      other_parts.scheme != url::kFileScheme) {
    return ContentSettingsPattern::IDENTITY;
  }

  if (parts.is_path_wildcard && !other_parts.is_path_wildcard)
    return ContentSettingsPattern::SUCCESSOR;
  if (!parts.is_path_wildcard && other_parts.is_path_wildcard)
    return ContentSettingsPattern::PREDECESSOR;

  int result = parts.path.compare(other_parts.path);
  if (result == 0)
    return ContentSettingsPattern::IDENTITY;
  if (result > 0)
    return ContentSettingsPattern::DISJOINT_ORDER_PRE;
  return ContentSettingsPattern::DISJOINT_ORDER_POST;
}
