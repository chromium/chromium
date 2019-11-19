// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_UTIL_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/policy/policy_export.h"
#include "components/url_matcher/url_matcher.h"

class GURL;

namespace policy {
namespace url_util {

// Normalizes a URL for matching purposes.
POLICY_EXPORT GURL Normalize(const GURL& url);

// Helper function to extract the underlying URL wrapped by services such as
// Google AMP or Google Translate. Returns an empty GURL if |url| doesn't match
// a known format.
POLICY_EXPORT GURL GetEmbeddedURL(const GURL& url);

struct FilterComponents {
  FilterComponents();
  FilterComponents(const FilterComponents&) = delete;
  FilterComponents(FilterComponents&&);
  FilterComponents& operator=(const FilterComponents&) = delete;
  FilterComponents& operator=(FilterComponents&&) = default;

  ~FilterComponents();

  // Returns true if |this| represents the "*" filter in the blacklist.
  bool IsBlacklistWildcard() const;

  std::string scheme;
  std::string host;
  uint16_t port;
  std::string path;
  std::string query;
  int number_of_key_value_pairs;
  bool match_subdomains;
  bool allow;
};

// Creates a condition set that can be used with the |url_matcher|. |id| needs
// to be a unique number that will be returned by the |url_matcher| if the URL
// matches that condition set. |allow| indicates if it is a white-list (true)
// or black-list (false) filter.
POLICY_EXPORT scoped_refptr<url_matcher::URLMatcherConditionSet>
CreateConditionSet(url_matcher::URLMatcher* url_matcher,
                   url_matcher::URLMatcherConditionSet::ID id,
                   const std::string& scheme,
                   const std::string& host,
                   bool match_subdomains,
                   uint16_t port,
                   const std::string& path,
                   const std::string& query,
                   bool allow);

// Splits a URL filter into its components. A GURL isn't used because these
// can be invalid URLs e.g. "google.com".
// Returns false if the URL couldn't be parsed. In case false is returned,
// the values of output parameters are undefined.
// The |host| is preprocessed so it can be passed to URLMatcher for the
// appropriate condition.
// The optional username and password are ignored.
// |match_subdomains| specifies whether the filter should include subdomains
// of the hostname (if it is one.)
// |port| is 0 if none is explicitly defined.
// |path| does not include query parameters.
// |query| contains the query parameters ('?' not included).
// All arguments are mandatory.
POLICY_EXPORT bool FilterToComponents(const std::string& filter,
                                      std::string* scheme,
                                      std::string* host,
                                      bool* match_subdomains,
                                      uint16_t* port,
                                      std::string* path,
                                      std::string* query);

// Adds the filters in |list| to |url_matcher| as a ConditionSet::Vector.
POLICY_EXPORT void AddFilters(
    url_matcher::URLMatcher* matcher,
    bool allow,
    url_matcher::URLMatcherConditionSet::ID* id,
    const base::ListValue* patterns,
    std::map<url_matcher::URLMatcherConditionSet::ID,
             url_util::FilterComponents>* filters = nullptr);

POLICY_EXPORT void AddAllowFilters(url_matcher::URLMatcher* matcher,
                                   const base::ListValue* patterns);

}  // namespace url_util
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_UTIL_H_
