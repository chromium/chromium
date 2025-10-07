// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_url_matcher.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/farmhash/src/src/farmhash.h"

namespace lens {

namespace {

// Converts a JSON string array to a vector.
std::vector<std::string> JSONArrayToVector(const std::string& json_array) {
  std::optional<base::Value> json_value =
      base::JSONReader::Read(json_array, base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  if (!json_value) {
    return {};
  }

  base::Value::List* entries = json_value->GetIfList();
  if (!entries) {
    return {};
  }

  std::vector<std::string> result;
  result.reserve(entries->size());
  for (const base::Value& entry : *entries) {
    const std::string* filter = entry.GetIfString();
    if (filter) {
      result.emplace_back(*filter);
    }
  }
  return result;
}

}  // namespace

LensUrlMatcher::LensUrlMatcher(std::string url_allow_filters,
                               std::string url_block_filters,
                               std::string path_match_allow_filters,
                               std::string path_match_block_filters,
                               std::string url_forced_allowed_match_patterns,
                               std::string hashed_domain_block_filters_list) {
  base::MatcherStringPattern::ID id(0);
  InitializeUrlMatcher(url_allow_filters, url_block_filters, &id);
  InitializeForceAllowUrlPatterns(url_forced_allowed_match_patterns, &id);
  InitializePathAllowMatcher(path_match_allow_filters, &id);
  InitializePathBlockMatcher(path_match_block_filters, &id);
  InitializeHashedDomainBlockFilters(hashed_domain_block_filters_list);
}

LensUrlMatcher::~LensUrlMatcher() = default;

void LensUrlMatcher::InitializeUrlMatcher(std::string url_allow_filters,
                                          std::string url_block_filters,
                                          base::MatcherStringPattern::ID* id) {
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddFiltersWithLimit(url_matcher_.get(), true, id,
                                         JSONArrayToVector(url_allow_filters),
                                         &url_filters_);
  url_matcher::util::AddFiltersWithLimit(url_matcher_.get(), false, id,
                                         JSONArrayToVector(url_block_filters),
                                         &url_filters_);
}

void LensUrlMatcher::InitializeForceAllowUrlPatterns(
    std::string url_path_forced_allowed_match_patterns,
    base::MatcherStringPattern::ID* id) {
  auto force_allow_url_strings =
      JSONArrayToVector(url_path_forced_allowed_match_patterns);
  std::vector<base::MatcherStringPattern> force_allow_url_patterns;
  std::vector<const base::MatcherStringPattern*> force_allow_url_pointers;
  force_allow_url_patterns.reserve(force_allow_url_strings.size());
  force_allow_url_pointers.reserve(force_allow_url_strings.size());
  for (const std::string& entry : force_allow_url_strings) {
    (*id)++;
    force_allow_url_patterns.emplace_back(entry, *id);
    force_allow_url_pointers.push_back(&force_allow_url_patterns.back());
  }
  url_forced_allow_matcher = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  url_forced_allow_matcher->AddPatterns(force_allow_url_pointers);
}

void LensUrlMatcher::InitializePathAllowMatcher(
    std::string path_match_allow_filters,
    base::MatcherStringPattern::ID* id) {
  const auto allow_strings = JSONArrayToVector(path_match_allow_filters);
  std::vector<base::MatcherStringPattern> allow_patterns;
  std::vector<const base::MatcherStringPattern*> allow_pointers;
  allow_patterns.reserve(allow_strings.size());
  allow_pointers.reserve(allow_strings.size());
  for (const std::string& entry : allow_strings) {
    (*id)++;
    allow_patterns.emplace_back(entry, *id);
    allow_pointers.push_back(&allow_patterns.back());
  }
  path_allow_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  path_allow_matcher_->AddPatterns(allow_pointers);
}

void LensUrlMatcher::InitializePathBlockMatcher(
    std::string path_match_block_filters,
    base::MatcherStringPattern::ID* id) {
  const auto block_strings = JSONArrayToVector(path_match_block_filters);
  std::vector<base::MatcherStringPattern> block_patterns;
  std::vector<const base::MatcherStringPattern*> block_pointers;
  block_patterns.reserve(block_strings.size());
  block_pointers.reserve(block_strings.size());
  for (const std::string& entry : block_strings) {
    (*id)++;
    block_patterns.emplace_back(entry, *id);
    block_pointers.push_back(&block_patterns.back());
  }
  path_block_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  path_block_matcher_->AddPatterns(block_pointers);
}

void LensUrlMatcher::InitializeHashedDomainBlockFilters(
    std::string hashed_domain_block_filters_list) {
  for (std::string_view hash_string :
       base::SplitStringPiece(hashed_domain_block_filters_list, ",",
                              base::WhitespaceHandling::TRIM_WHITESPACE,
                              base::SplitResult::SPLIT_WANT_NONEMPTY)) {
    uint32_t hash;
    if (base::StringToUint(hash_string, &hash)) {
      hashed_domain_block_filters_.insert(hash);
    }
  }
}

bool LensUrlMatcher::IsMatch(const GURL& url) {
  // Check if the URL matches any of the allow filters. If it does not, return
  // false immediately to block this URL.
  auto matches = url_matcher_.get()->MatchURL(url);
  if (!matches.size()) {
    return false;
  }

  // Now that the URL is allowed, check if it matches any of the block filters.
  // If it does, return false to block this URL.
  for (auto match : matches) {
    // Blocks take precedence over allows.
    if (!url_filters_[match].allow) {
      return false;
    }
  }

  // Check if the domain matches any of the hashed block filters. If it does,
  // return false to block this URL.
  if (SubdomainsMatchHash(url.GetHost())) {
    return false;
  }

  // Check if the path matches the path block matcher. If it does, return false
  // to block this URL.
  if (path_block_matcher_ && !path_block_matcher_->IsEmpty() &&
      path_block_matcher_->Match(url.GetPath(), &matches)) {
    return false;
  }

  // Check if the URL matches any of the forced allowed URLs. If it does, return
  // true as this should be a shown match even if the path does not contain an
  // allowlisted pattern (below).
  if (url_forced_allow_matcher && !url_forced_allow_matcher->IsEmpty() &&
      url_forced_allow_matcher->Match(url.spec(), &matches)) {
    return true;
  }

  // Finally, check if the path matches the path allow matcher. If it doesn't,
  // return false to block this URL.
  if (path_allow_matcher_ && !path_allow_matcher_->IsEmpty() &&
      !path_allow_matcher_->Match(url.GetPath(), &matches)) {
    return false;
  }

  // Finally if all checks pass, this must be a valid match, i.e.:
  // 1. The URL matches at least one of the allowed URLs.
  // 2. The URL does not match any of the blocked URLs.
  // 3. The domain does not match any of the hashed blocked domains.
  // 4. The URL does not match any of the block path patterns.
  // 5. The URL either matches the force allowed patterns, or matches at least
  //    one of the allowed path patterns.
  return true;
}

bool LensUrlMatcher::SubdomainsMatchHash(std::string_view str) {
  // Remove any periods from the start and end of the hostname.
  size_t start = str.find_first_not_of('.');
  if (start == std::string::npos) {
    return false;
  }
  size_t end = str.find_last_not_of('.');
  std::string_view domain =
      std::string_view(str).substr(start, 1 + end - start);
  while (true) {
    if (MatchesHash(domain)) {
      return true;
    }
    size_t found = domain.find('.');
    if (found == std::string::npos) {
      // Top-level domain.
      return false;
    }
    domain = domain.substr(found + 1);
  }
}

bool LensUrlMatcher::MatchesHash(std::string_view str) {
  uint32_t hash = util::Fingerprint32(str);
  return hashed_domain_block_filters_.contains(hash);
}

}  // namespace lens
