// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_URL_MATCHER_H_
#define CHROME_BROWSER_UI_LENS_LENS_URL_MATCHER_H_

#include <string>
#include <unordered_set>

#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace lens {

class LensUrlMatcher {
 public:
  LensUrlMatcher(std::string url_allow_filters,
                 std::string url_block_filters,
                 std::string path_match_allow_filters,
                 std::string path_match_block_filters,
                 std::string path_forced_allowed_match_patterns,
                 std::string hashed_domain_block_filters_list);
  ~LensUrlMatcher();

  // True if the given URL is allowed and not blocked by the filters with which
  // the LensUrlMatcher was initialized. See unit test for examples.
  bool IsMatch(const GURL& url);

 private:
  void InitializeUrlMatcher(std::string url_allow_filters,
                            std::string url_block_filters,
                            base::MatcherStringPattern::ID* id);
  void InitializeForceAllowUrlPatterns(
      std::string path_forced_allowed_match_patterns,
      base::MatcherStringPattern::ID* id);
  void InitializePathAllowMatcher(std::string path_match_allow_filters,
                                  base::MatcherStringPattern::ID* id);
  void InitializePathBlockMatcher(std::string path_match_block_filters,
                                  base::MatcherStringPattern::ID* id);
  void InitializeHashedDomainBlockFilters(
      std::string hashed_domain_block_filters_list);
  bool SubdomainsMatchHash(std::string_view str);
  bool MatchesHash(std::string_view str);

  // URLMatcher which uses the URLBlocklist format to allow or block URLs.
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;

  // Matcher for URLs that do not need to pass the check for allowed paths.
  // Instead, if they match the url_matcher_ and do not contain any of the
  // blocked paths, they are considered matches.
  std::unique_ptr<url_matcher::RegexSetMatcher> url_forced_allow_matcher;

  // Matcher for URL paths that are allowed.
  std::unique_ptr<url_matcher::RegexSetMatcher> path_allow_matcher_;

  // Matcher for URL paths that are blocked.
  std::unique_ptr<url_matcher::RegexSetMatcher> path_block_matcher_;

  // Hashed domain block filters.
  std::unordered_set<uint32_t> hashed_domain_block_filters_;

  // Filters used by the URL matcher. Used to look up if a matching filter is an
  // allow filter or a block filter.
  std::map<base::MatcherStringPattern::ID, url_matcher::util::FilterComponents>
      url_filters_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_URL_MATCHER_H_
