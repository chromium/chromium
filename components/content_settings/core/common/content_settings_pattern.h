// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Patterns used in content setting rules.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PATTERN_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PATTERN_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

class GURL;

namespace content_settings {
class PatternParser;

namespace mojom {
class ContentSettingsPatternDataView;
}
}  // namespace content_settings

// A pattern used in content setting rules. See |IsValid| for a description of
// possible patterns.
class ContentSettingsPattern {
 public:
  // Each content settings pattern describes a set of origins. Patterns, and the
  // sets they describe, have specific relations. |Relation| describes the
  // relation of two patterns A and B. When pattern A is compared with pattern B
  // (A compare B) interesting relations are:
  // - IDENTITY:
  //   Pattern A and B are identical. The patterns are equal.
  //
  // - DISJOINT_ORDER_PRE:
  //   Pattern A and B have no intersection. A and B never match the origin of
  //   a URL at the same time. But pattern A has a higher precedence than
  //   pattern B when patterns are sorted.
  //
  // - DISJOINT_ORDER_POST:
  //   Pattern A and B have no intersection. A and B never match the origin of
  //   a URL at the same time. But pattern A has a lower precedence than
  //   pattern B when patterns are sorted.
  //
  // - SUCCESSOR:
  //   Pattern A and B have an intersection. But pattern B has a higher
  //   precedence than pattern A for URLs that are matched by both pattern.
  //
  // - PREDECESSOR:
  //   Pattern A and B have an intersection. But pattern A has a higher
  //   precedence than pattern B for URLs that are matched by both pattern.
  //
  //  See the url below for more details about pattern precedence.
  //  https://developer.chrome.com/docs/extensions/reference/api/contentSettings#content_setting_patterns
  //
  enum Relation {
    DISJOINT_ORDER_POST = -2,
    SUCCESSOR = -1,
    IDENTITY = 0,
    PREDECESSOR = 1,
    DISJOINT_ORDER_PRE = 2,
  };

  // This enum is used to back an UMA histogram, the order of existing values
  // should not be changed.
  // New values should only be appended before SCHEME_MAX.
  // Also keep it consistent with kSchemeNames in content_settings_pattern.cc,
  // and the ContentSettingScheme enum in histograms/enums.xml.
  enum SchemeType {
    SCHEME_WILDCARD,
    SCHEME_OTHER,
    SCHEME_HTTP,
    SCHEME_HTTPS,
    SCHEME_FILE,
    SCHEME_CHROMEEXTENSION,
    SCHEME_CHROMESEARCH,
    SCHEME_CHROME,
    SCHEME_CHROMEUNTRUSTED,
    SCHEME_DEVTOOLS,
    SCHEME_ISOLATEDAPP,
    SCHEME_MAX,
  };

  struct PatternParts {
    PatternParts();
    PatternParts(const PatternParts& other);
    PatternParts(PatternParts&& other);
    ~PatternParts();

    PatternParts& operator=(const PatternParts& other);
    PatternParts& operator=(PatternParts&& other);

    bool operator==(const PatternParts& other) const;

    // Lowercase string of the URL scheme to match. This string is empty if the
    // |is_scheme_wildcard| flag is set.
    std::string scheme;

    // True if the scheme wildcard is set.
    bool is_scheme_wildcard;

    // Normalized string that is either of the following:
    // - IPv4 or IPv6
    // - hostname
    // - domain
    // - empty string if the |is_host_wildcard| flag is set.
    std::string host;

    // True if the domain wildcard is set.
    bool has_domain_wildcard;

    // String with the port to match. This string is empty if the
    // |is_port_wildcard| flag is set.
    std::string port;

    // True if the port wildcard is set.
    bool is_port_wildcard;

    // TODO(markusheintz): Needed for legacy reasons. Remove. Path
    // specification. Only used for content settings pattern with a "file"
    // scheme part.
    std::string path;

    // True if the path wildcard is set.
    bool is_path_wildcard;
  };

  class BuilderInterface {
   public:
    virtual ~BuilderInterface() = default;

    virtual BuilderInterface* WithPort(const std::string& port) = 0;

    virtual BuilderInterface* WithPortWildcard() = 0;

    virtual BuilderInterface* WithHost(const std::string& host) = 0;

    virtual BuilderInterface* WithDomainWildcard() = 0;

    virtual BuilderInterface* WithScheme(const std::string& scheme) = 0;

    virtual BuilderInterface* WithSchemeWildcard() = 0;

    virtual BuilderInterface* WithPath(const std::string& path) = 0;

    virtual BuilderInterface* WithPathWildcard() = 0;

    virtual BuilderInterface* Invalid() = 0;

    // Returns a content settings pattern according to the current configuration
    // of the builder.
    virtual ContentSettingsPattern Build() = 0;
  };

  static std::unique_ptr<BuilderInterface> CreateBuilder();

  // The version of the pattern format implemented.
  static const int kContentSettingsPatternVersion;

  // Returns a wildcard content settings pattern that matches all possible valid
  // origins.
  static ContentSettingsPattern Wildcard();

  // Returns a pattern that matches the scheme and host of this URL, as well as
  // all subdomains and ports.
  static ContentSettingsPattern FromURL(const GURL& url);

  // Returns a pattern that matches exactly this URL. (Paths are ignored for
  // non-"file://" URLs.)
  static ContentSettingsPattern FromURLNoWildcard(const GURL& url);

  // Converts a given url to a ContentSettingsPattern that represents a site,
  // i.e. with domain, path, and port wildcards.
  static ContentSettingsPattern FromURLToSchemefulSitePattern(const GURL& url);

  // Returns a pattern that matches the given pattern specification.
  // Valid patterns specifications are:
  //   - [*.]domain.tld (matches domain.tld and all sub-domains)
  //   - host (matches an exact hostname)
  //   - scheme://host:port (supported schemes: http,https)
  //   - scheme://[*.]domain.tld:port (supported schemes: http,https)
  //   - file://path (The path has to be an absolute path and start with a '/')
  //   - a.b.c.d (matches an exact IPv4 ip)
  //   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
  static ContentSettingsPattern FromString(std::string_view pattern_spec);

  // Sets schemes that do not support domain wildcards and ports.
  // Needs to be called by the embedder before using ContentSettingsPattern.
  // |schemes| can't be NULL, and the pointed to strings must remain alive
  // until the app terminates.
  // The method should only be called once. If called again, the parameters
  // must have values equal to the parameter values of the first call.
  // The |count| parameter represents the number of strings that
  // |schemes| points to.
  static void SetNonWildcardDomainNonPortSchemes(const char* const* schemes,
                                                 size_t count);

  // Compares |scheme| against the schemes set by the embedder.
  static bool IsNonWildcardDomainNonPortScheme(std::string_view scheme);

  // Convert pattern to domain wildcard pattern. If fail to extract domain from
  // the pattern, return an invalid pattern.
  static ContentSettingsPattern ToDomainWildcardPattern(
      const ContentSettingsPattern& pattern);

  // Convert pattern to host only pattern.
  static ContentSettingsPattern ToHostOnlyPattern(
      const ContentSettingsPattern& pattern);

  // Expose a comparator to sort domains by precedence. Highest precedence
  // first. Returns true if |domain_a| has a higher precedence than |domain_b|.
  // If there is no difference in precedence, then the domains are compared
  // alphabetically.
  struct CompareDomains {
    using is_transparent = void;
    bool operator()(std::string_view domain_a, std::string_view domain_b) const;
  };

  // Constructs an empty pattern. Empty patterns are invalid patterns. Invalid
  // patterns match nothing.
  ContentSettingsPattern();

  // True if this is a valid pattern.
  bool IsValid() const { return is_valid_; }

  // True if |url| matches this pattern.
  bool Matches(const GURL& url) const;

  // True if this pattern matches all hosts (i.e. it has a host wildcard).
  bool MatchesAllHosts() const;

  // True if this pattern matches a single origin (i.e. it's the narrowest kind
  // of a pattern, with no wildcards).
  bool MatchesSingleOrigin() const;

  // True if this pattern has domain wildcard.
  bool HasDomainWildcard() const;

  // Returns a std::string representation of this pattern.
  std::string ToString() const;

  // Returns a valid URL that matches the pattern if a host part is specified.
  // If the pattern matches a file:// scheme, the path needs to be specified.
  // Returns GURL() otherwise.
  GURL ToRepresentativeUrl() const;

  // Returns scheme type of pattern.
  ContentSettingsPattern::SchemeType GetScheme() const;

  // Returns the host of a pattern.
  const std::string& GetHost() const;

  // Compares the pattern with a given |other| pattern and returns the
  // |Relation| of the two patterns.
  Relation Compare(const ContentSettingsPattern& other) const;

  // Returns true if the pattern and the |other| pattern are identical.
  bool operator==(const ContentSettingsPattern& other) const;

  // Returns true if the pattern and the |other| pattern are not identical.
  bool operator!=(const ContentSettingsPattern& other) const;

  // Returns true if the pattern has a lower priority than the |other| pattern.
  bool operator<(const ContentSettingsPattern& other) const;

  // Returns true if the pattern has a higher priority than the |other| pattern.
  bool operator>(const ContentSettingsPattern& other) const;

  // Formatter method for Google Test
  friend void PrintTo(const ContentSettingsPattern& pattern, std::ostream* os) {
    *os << pattern.ToString();
  }

 private:
  friend class content_settings::PatternParser;
  friend struct mojo::StructTraits<
      content_settings::mojom::ContentSettingsPatternDataView,
      ContentSettingsPattern>;
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsPatternParserTest, SerializePatterns);

  class Builder;

  static Relation CompareScheme(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  static Relation CompareHost(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  static Relation ComparePort(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  static Relation ComparePath(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  ContentSettingsPattern(PatternParts parts, bool valid);

  PatternParts parts_;

  bool is_valid_;
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PATTERN_H_
