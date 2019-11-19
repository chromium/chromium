// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_FILTER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_FILTER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/bloom_filter.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace optimization_guide {

typedef std::vector<std::unique_ptr<re2::RE2>> RegexpList;

// OptimizationFilter represents a filter with two underlying implementations: a
// Bloom filter and a set of regexps. This class has a 1:1 mapping with an
// OptimizationFilter protobuf message where this is the logical implementation
// of the proto data.
//
// TODO(dougarnett): consider moving this and BloomFilter under
// components/blacklist/.
class OptimizationFilter {
 public:
  explicit OptimizationFilter(std::unique_ptr<BloomFilter> bloom_filter,
                              std::unique_ptr<RegexpList> regexps);

  ~OptimizationFilter();

  // Returns true if the given url is matched by this filter.
  bool Matches(const GURL& url) const;

 private:
  // Returns whether this filter contains a host suffix for the host part
  // of |url|. It will check at most 5 host suffixes and it may ignore simple
  // top level domain matches (such as "com" or "co.in").
  //
  // A host suffix is comprised of domain name level elements (vs. characters).
  // For example, "server1.www.company.co.in" has the following suffixes that
  // would be checked for membership:
  //   "server1.www.company.co.in"
  //   "www.company.co.in"
  //   "company.co.in"
  // This method will return true if any of those suffixes are present.
  bool ContainsHostSuffix(const GURL& url) const;

  // Returns whether this filter contains a regexp that matches the given url.
  bool MatchesRegexp(const GURL& url) const;

  std::unique_ptr<BloomFilter> bloom_filter_;

  std::unique_ptr<RegexpList> regexps_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(OptimizationFilter);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_FILTER_H_
