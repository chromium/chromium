// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_filter.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

// Maximum number of suffixes to check per url.
const int kMaxSuffixCount = 5;

// Realistic minimum length of a host suffix.
const int kMinHostSuffix = 6;  // eg., abc.tv

bool MatchesRegexp(const GURL& url, const RegexpList& regexps) {
  if (!url.is_valid())
    return false;

  GURL::Replacements replace_url_auth;
  replace_url_auth.ClearUsername();
  replace_url_auth.ClearPassword();
  std::string clean_url =
      base::ToLowerASCII(url.ReplaceComponents(replace_url_auth).spec());

  for (auto& regexp : regexps) {
    if (!regexp->ok()) {
      continue;
    }

    if (re2::RE2::PartialMatch(clean_url, *regexp)) {
      return true;
    }
  }

  return false;
}

// Returns a SHA256 hex string for the given input.
std::string SHA256(std::string_view input) {
  uint8_t result[crypto::kSHA256Length];
  crypto::SHA256HashString(input, result, std::size(result));
  return base::HexEncode(result);
}

}  // namespace

OptimizationFilter::OptimizationFilter(
    std::unique_ptr<BloomFilter> bloom_filter,
    std::unique_ptr<RegexpList> regexps,
    std::unique_ptr<RegexpList> exclusion_regexps,
    bool skip_host_suffix_checking,
    proto::BloomFilterFormat bloom_filter_format)
    : bloom_filter_(std::move(bloom_filter)),
      regexps_(std::move(regexps)),
      exclusion_regexps_(std::move(exclusion_regexps)),
      skip_host_suffix_checking_(skip_host_suffix_checking),
      bloom_filter_format_(bloom_filter_format) {
  // May be created on one thread but used on another. The first call to
  // CalledOnValidSequence() will re-bind it.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

OptimizationFilter::~OptimizationFilter() = default;

bool OptimizationFilter::Matches(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (exclusion_regexps_ && MatchesRegexp(url, *exclusion_regexps_))
    return false;
  return ContainsHostSuffix(url) || (regexps_ && MatchesRegexp(url, *regexps_));
}

bool OptimizationFilter::ContainsHostSuffix(const GURL& url) const {
  if (!bloom_filter_)
    return false;

  // First check full host name.
  if (bloom_filter_format_ == proto::BLOOM_FILTER_FORMAT_SHA256) {
    if (bloom_filter_->Contains(SHA256(url.host()))) {
      return true;
    }
  } else {
    if (bloom_filter_->Contains(url.host())) {
      return true;
    }
  }

  // Do not check host suffixes if we are told to skip host suffix checking.
  if (skip_host_suffix_checking_)
    return false;

  // Now check host suffixes from shortest to longest but skipping the
  // root domain (eg, skipping "com", "org", "in", "uk").
  std::string full_host(url.host());
  int suffix_count = 1;
  auto left_pos = full_host.find_last_of('.');  // root domain position
  while ((left_pos - 1) != std::string::npos &&
         (left_pos = full_host.find_last_of('.', left_pos - 1)) !=
             std::string::npos &&
         suffix_count < kMaxSuffixCount) {
    if (full_host.length() - left_pos > kMinHostSuffix) {
      std::string suffix = full_host.substr(left_pos + 1);
      suffix_count++;

      if (bloom_filter_format_ == proto::BLOOM_FILTER_FORMAT_SHA256) {
        if (bloom_filter_->Contains(SHA256(suffix))) {
          return true;
        }
      } else {
        if (bloom_filter_->Contains(suffix)) {
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace optimization_guide
