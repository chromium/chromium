// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/url_util.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/google/core/common/google_util.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_matcher/url_matcher.h"
#include "net/base/filename_util.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using url_matcher::URLMatcher;
using url_matcher::URLMatcherCondition;
using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherPortFilter;
using url_matcher::URLMatcherSchemeFilter;
using url_matcher::URLQueryElementMatcherCondition;

namespace url_matcher {
namespace util {

namespace {

// Host/regex pattern for Google AMP Cache URLs.
// See https://developers.google.com/amp/cache/overview#amp-cache-url-format
// for a definition of the format of AMP Cache URLs.
const char kGoogleAmpCacheHost[] = "cdn.ampproject.org";
const char kGoogleAmpCachePathPattern[] = "/[a-z]/(s/)?(.*)";

// Regex pattern for the path of Google AMP Viewer URLs.
const char kGoogleAmpViewerPathPattern[] = "/amp/(s/)?(.*)";

// Host, path prefix, and query regex pattern for Google web cache URLs.
const char kGoogleWebCacheHost[] = "webcache.googleusercontent.com";
const char kGoogleWebCachePathPrefix[] = "/search";
const char kGoogleWebCacheQueryPattern[] =
    "cache:(.{12}:)?(https?://)?([^ :]*)( [^:]*)?";

const char kGoogleTranslateSubdomain[] = "translate.";
const char kAlternateGoogleTranslateHost[] = "translate.googleusercontent.com";

// Maximum filters allowed. Filters over this index are ignored.
const size_t kMaxFiltersAllowed = 1000;

// Returns a full URL using either "http" or "https" as the scheme.
GURL BuildURL(bool is_https, const std::string& host_and_path) {
  std::string scheme = is_https ? url::kHttpsScheme : url::kHttpScheme;
  return GURL(scheme + "://" + host_and_path);
}

void ProcessQueryToConditions(
    url_matcher::URLMatcherConditionFactory* condition_factory,
    const std::string& query,
    bool allow,
    std::set<URLQueryElementMatcherCondition>* query_conditions) {
  url::Component query_left = url::MakeRange(0, query.length());
  url::Component key;
  url::Component value;
  // Depending on the filter type being block-list or allow-list, the matcher
  // choose any or every match. The idea is a URL should be blocked if
  // there is any occurrence of the key value pair. It should be allowed
  // only if every occurrence of the key is followed by the value. This avoids
  // situations such as a user appending an allowed video parameter in the
  // end of the query and watching a video of their choice (the last parameter
  // is ignored by some web servers like youtube's).
  URLQueryElementMatcherCondition::Type match_type =
      allow ? URLQueryElementMatcherCondition::MATCH_ALL
            : URLQueryElementMatcherCondition::MATCH_ANY;

  while (ExtractQueryKeyValue(query, &query_left, &key, &value)) {
    URLQueryElementMatcherCondition::QueryElementType query_element_type =
        value.len ? URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY_VALUE
                  : URLQueryElementMatcherCondition::ELEMENT_TYPE_KEY;
    URLQueryElementMatcherCondition::QueryValueMatchType query_value_match_type;
    if (!value.len && key.len && query[key.end() - 1] == '*') {
      --key.len;
      query_value_match_type =
          URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX;
    } else if (value.len && query[value.end() - 1] == '*') {
      --value.len;
      query_value_match_type =
          URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_PREFIX;
    } else {
      query_value_match_type =
          URLQueryElementMatcherCondition::QUERY_VALUE_MATCH_EXACT;
    }
    query_conditions->insert(URLQueryElementMatcherCondition(
        query.substr(key.begin, key.len), query.substr(value.begin, value.len),
        query_value_match_type, query_element_type, match_type,
        condition_factory));
  }
}

// Helper class for testing the URL against precompiled regexes. This is a
// singleton so the cached regexes are only created once.
class EmbeddedURLExtractor {
 public:
  EmbeddedURLExtractor(const EmbeddedURLExtractor&) = delete;
  EmbeddedURLExtractor& operator=(const EmbeddedURLExtractor&) = delete;

  static EmbeddedURLExtractor* GetInstance() {
    static base::NoDestructor<EmbeddedURLExtractor> instance;
    return instance.get();
  }

  // Implements url_filter::GetEmbeddedURL().
  GURL GetEmbeddedURL(const GURL& url) {
    // Check for "*.cdn.ampproject.org" URLs.
    if (url.DomainIs(kGoogleAmpCacheHost)) {
      std::string s;
      std::string embedded;
      if (re2::RE2::FullMatch(url.path(), google_amp_cache_path_regex_, &s,
                              &embedded)) {
        if (url.has_query())
          embedded += "?" + url.query();
        return BuildURL(!s.empty(), embedded);
      }
    }

    // Check for "www.google.TLD/amp/" URLs.
    if (google_util::IsGoogleDomainUrl(
            url, google_util::DISALLOW_SUBDOMAIN,
            google_util::DISALLOW_NON_STANDARD_PORTS)) {
      std::string s;
      std::string embedded;
      if (re2::RE2::FullMatch(url.path(), google_amp_viewer_path_regex_, &s,
                              &embedded)) {
        // The embedded URL may be percent-encoded. Undo that.
        embedded = base::UnescapeBinaryURLComponent(embedded);
        return BuildURL(!s.empty(), embedded);
      }
    }

    // Check for Google web cache URLs
    // ("webcache.googleusercontent.com/search?q=cache:...").
    std::string query;
    if (url.host_piece() == kGoogleWebCacheHost &&
        base::StartsWith(url.path_piece(), kGoogleWebCachePathPrefix) &&
        net::GetValueForKeyInQuery(url, "q", &query)) {
      std::string fingerprint;
      std::string scheme;
      std::string embedded;
      if (re2::RE2::FullMatch(query, google_web_cache_query_regex_,
                              &fingerprint, &scheme, &embedded)) {
        return BuildURL(scheme == "https://", embedded);
      }
    }

    // Check for Google translate URLs ("translate.google.TLD/...?...&u=URL" or
    // "translate.googleusercontent.com/...?...&u=URL").
    bool is_translate = false;
    if (base::StartsWith(url.host_piece(), kGoogleTranslateSubdomain)) {
      // Remove the "translate." prefix.
      GURL::Replacements replace;
      replace.SetHostStr(
          url.host_piece().substr(strlen(kGoogleTranslateSubdomain)));
      GURL trimmed = url.ReplaceComponents(replace);
      // Check that the remainder is a Google URL. Note: IsGoogleDomainUrl
      // checks for [www.]google.TLD, but we don't want the "www.", so
      // explicitly exclude that.
      // TODO(treib,pam): Instead of excluding "www." manually, teach
      // IsGoogleDomainUrl a mode that doesn't allow it.
      is_translate = google_util::IsGoogleDomainUrl(
                         trimmed, google_util::DISALLOW_SUBDOMAIN,
                         google_util::DISALLOW_NON_STANDARD_PORTS) &&
                     !base::StartsWith(trimmed.host_piece(), "www.");
    }
    bool is_alternate_translate =
        url.host_piece() == kAlternateGoogleTranslateHost;
    if (is_translate || is_alternate_translate) {
      std::string embedded;
      if (net::GetValueForKeyInQuery(url, "u", &embedded)) {
        // The embedded URL may or may not include a scheme. Fix it if
        // necessary.
        return url_formatter::FixupURL(embedded, /*desired_tld=*/std::string());
      }
    }

    return GURL();
  }

 private:
  friend class base::NoDestructor<EmbeddedURLExtractor>;

  EmbeddedURLExtractor()
      : google_amp_cache_path_regex_(kGoogleAmpCachePathPattern),
        google_amp_viewer_path_regex_(kGoogleAmpViewerPathPattern),
        google_web_cache_query_regex_(kGoogleWebCacheQueryPattern) {
    DCHECK(google_amp_cache_path_regex_.ok());
    DCHECK(google_amp_viewer_path_regex_.ok());
    DCHECK(google_web_cache_query_regex_.ok());
  }

  ~EmbeddedURLExtractor() = default;

  const re2::RE2 google_amp_cache_path_regex_;
  const re2::RE2 google_amp_viewer_path_regex_;
  const re2::RE2 google_web_cache_query_regex_;
};

}  // namespace

// Converts a ValueList |value| of strings into a vector. Returns true if
// successful.
bool GetAsStringVector(const base::Value* value,
                       std::vector<std::string>* out) {
  if (!value->is_list())
    return false;

  for (const base::Value& item : value->GetList()) {
    if (!item.is_string())
      return false;

    out->push_back(item.GetString());
  }
  return true;
}

GURL Normalize(const GURL& url) {
  GURL normalized_url = url;
  GURL::Replacements replacements;
  // Strip username, password, query, and ref.
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

GURL GetEmbeddedURL(const GURL& url) {
  return EmbeddedURLExtractor::GetInstance()->GetEmbeddedURL(url);
}

size_t GetMaxFiltersAllowed() {
  return kMaxFiltersAllowed;
}

FilterComponents::FilterComponents() = default;
FilterComponents::~FilterComponents() = default;
FilterComponents::FilterComponents(FilterComponents&&) = default;

bool FilterComponents::IsWildcard() const {
  return host.empty() && scheme.empty() && path.empty() && query.empty() &&
         port == 0 && number_of_url_matching_conditions == 0 &&
         match_subdomains;
}

scoped_refptr<URLMatcherConditionSet> CreateConditionSet(
    URLMatcher* url_matcher,
    base::MatcherStringPattern::ID id,
    const std::string& scheme,
    const std::string& host,
    bool match_subdomains,
    uint16_t port,
    const std::string& path,
    const std::string& query,
    bool allow) {
  URLMatcherConditionFactory* condition_factory =
      url_matcher->condition_factory();
  std::set<URLMatcherCondition> conditions;
  conditions.insert(
      match_subdomains
          ? condition_factory->CreateHostSuffixPathPrefixCondition(host, path)
          : condition_factory->CreateHostEqualsPathPrefixCondition(host, path));

  std::set<URLQueryElementMatcherCondition> query_conditions;
  if (!query.empty()) {
    ProcessQueryToConditions(condition_factory, query, allow,
                             &query_conditions);
  }

  std::unique_ptr<URLMatcherSchemeFilter> scheme_filter;
  if (!scheme.empty())
    scheme_filter = std::make_unique<URLMatcherSchemeFilter>(scheme);

  std::unique_ptr<URLMatcherPortFilter> port_filter;
  if (port != 0) {
    std::vector<URLMatcherPortFilter::Range> ranges;
    ranges.push_back(URLMatcherPortFilter::CreateRange(port));
    port_filter = std::make_unique<URLMatcherPortFilter>(ranges);
  }

  return base::MakeRefCounted<URLMatcherConditionSet>(
      id, conditions, query_conditions, std::move(scheme_filter),
      std::move(port_filter));
}

bool FilterToComponents(const std::string& filter,
                        std::string* scheme,
                        std::string* host,
                        bool* match_subdomains,
                        uint16_t* port,
                        std::string* path,
                        std::string* query) {
  DCHECK(scheme);
  DCHECK(host);
  DCHECK(match_subdomains);
  DCHECK(port);
  DCHECK(path);
  DCHECK(query);
  url::Parsed parsed;
  std::string lc_filter = base::ToLowerASCII(filter);
  const std::string url_scheme = url_formatter::SegmentURL(filter, &parsed);

  // Check if it's a scheme wildcard pattern. We support both versions
  // (scheme:* and scheme://*) the later being consistent with old filter
  // definitions.
  if (lc_filter == url_scheme + ":*" || lc_filter == url_scheme + "://*") {
    scheme->assign(url_scheme);
    host->clear();
    *match_subdomains = true;
    *port = 0;
    path->clear();
    query->clear();
    return true;
  }

  if (url_scheme == url::kFileScheme) {
    base::FilePath file_path;
    if (!net::FileURLToFilePath(GURL(filter), &file_path))
      return false;

    *scheme = url::kFileScheme;
    host->clear();
    *match_subdomains = true;
    *port = 0;
    *path = file_path.AsUTF8Unsafe();
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Separators have to be canonicalized on Windows.
    std::replace(path->begin(), path->end(), '\\', '/');
    *path = "/" + *path;
#endif
    query->clear();
    return true;
  }

  if (url_scheme == url::kDataScheme) {
    *scheme = url::kDataScheme;
    host->clear();
    *match_subdomains = true;
    *port = 0;
    *path = GURL(filter).GetContent();
    query->clear();
    return true;
  }

  // According to documentation host can't be empty.
  if (parsed.host.is_empty())
    return false;

  if (parsed.scheme.is_nonempty())
    scheme->assign(url_scheme);
  else
    scheme->clear();

  host->assign(filter, parsed.host.begin, parsed.host.len);
  *host = base::ToLowerASCII(*host);
  // Special '*' host, matches all hosts.
  if (*host == "*") {
    host->clear();
    *match_subdomains = true;
  } else if (host->at(0) == '.') {
    // A leading dot in the pattern syntax means that we don't want to match
    // subdomains.
    host->erase(0, 1);
    *match_subdomains = false;
  } else {
    url::RawCanonOutputT<char> output;
    url::CanonHostInfo host_info;
    url::CanonicalizeHostVerbose(filter.c_str(), parsed.host, &output,
                                 &host_info);
    if (host_info.family == url::CanonHostInfo::NEUTRAL) {
      // We want to match subdomains. Add a dot in front to make sure we only
      // match at domain component boundaries.
      *host = "." + *host;
      *match_subdomains = true;
    } else {
      *match_subdomains = false;
    }
  }

  if (parsed.port.is_nonempty()) {
    int int_port;
    if (!base::StringToInt(filter.substr(parsed.port.begin, parsed.port.len),
                           &int_port)) {
      return false;
    }
    if (int_port <= 0 || int_port > std::numeric_limits<uint16_t>::max())
      return false;
    *port = int_port;
  } else {
    // Match any port.
    *port = 0;
  }

  if (parsed.path.is_nonempty())
    path->assign(filter, parsed.path.begin, parsed.path.len);
  else
    path->clear();

  if (parsed.query.is_nonempty())
    query->assign(filter, parsed.query.begin, parsed.query.len);
  else
    query->clear();

  return true;
}

void AddFilters(URLMatcher* matcher,
                bool allow,
                base::MatcherStringPattern::ID* id,
                const base::Value::List& patterns,
                std::map<base::MatcherStringPattern::ID,
                         url_matcher::util::FilterComponents>* filters) {
  URLMatcherConditionSet::Vector all_conditions;
  size_t size = std::min(kMaxFiltersAllowed, patterns.size());
  scoped_refptr<URLMatcherConditionSet> condition_set;
  for (size_t i = 0; i < size; ++i) {
    DCHECK(patterns[i].is_string());
    const std::string pattern = patterns[i].GetString();
    FilterComponents components;
    components.allow = allow;
    if (!FilterToComponents(pattern, &components.scheme, &components.host,
                            &components.match_subdomains, &components.port,
                            &components.path, &components.query)) {
      LOG(ERROR) << "Invalid pattern " << pattern;
      continue;
    }
    condition_set =
        CreateConditionSet(matcher, ++(*id), components.scheme, components.host,
                           components.match_subdomains, components.port,
                           components.path, components.query, allow);
    if (filters) {
      components.number_of_url_matching_conditions =
          condition_set->query_conditions().size();
      (*filters)[*id] = std::move(components);
    }
    all_conditions.push_back(std::move(condition_set));
  }
  matcher->AddConditionSets(all_conditions);
}

void AddFilters(URLMatcher* matcher,
                bool allow,
                base::MatcherStringPattern::ID* id,
                const std::vector<std::string>& patterns,
                std::map<base::MatcherStringPattern::ID,
                         url_matcher::util::FilterComponents>* filters) {
  URLMatcherConditionSet::Vector all_conditions;
  size_t size = std::min(kMaxFiltersAllowed, patterns.size());
  scoped_refptr<URLMatcherConditionSet> condition_set;
  for (size_t i = 0; i < size; ++i) {
    FilterComponents components;
    components.allow = allow;
    if (!FilterToComponents(patterns[i], &components.scheme, &components.host,
                            &components.match_subdomains, &components.port,
                            &components.path, &components.query)) {
      LOG(ERROR) << "Invalid pattern " << patterns[i];
      continue;
    }
    condition_set =
        CreateConditionSet(matcher, ++(*id), components.scheme, components.host,
                           components.match_subdomains, components.port,
                           components.path, components.query, allow);
    if (filters) {
      components.number_of_url_matching_conditions =
          condition_set->query_conditions().size();
      (*filters)[*id] = std::move(components);
    }
    all_conditions.push_back(std::move(condition_set));
  }
  matcher->AddConditionSets(all_conditions);
}

void AddAllowFilters(url_matcher::URLMatcher* matcher,
                     const base::Value::List& patterns) {
  base::MatcherStringPattern::ID id(0);
  AddFilters(matcher, true, &id, patterns);
}

void AddAllowFilters(url_matcher::URLMatcher* matcher,
                     const std::vector<std::string>& patterns) {
  base::MatcherStringPattern::ID id(0);
  AddFilters(matcher, true, &id, patterns);
}

}  // namespace util
}  // namespace url_matcher
