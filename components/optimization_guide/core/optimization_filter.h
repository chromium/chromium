// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_FILTER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_FILTER_H_

#include <memory>
#include <vector>

#include "base/sequence_checker.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace optimization_guide {

typedef std::vector<std::unique_ptr<re2::RE2>> RegexpList;

// OptimizationFilter represents a filter with two underlying implementations: a
// Bloom filter and sets of regexps. This class has a 1:1 mapping with an
// OptimizationFilter protobuf message where this is the logical implementation
// of the proto data.
class OptimizationFilter {
 public:
  OptimizationFilter(std::unique_ptr<BloomFilter> bloom_filter,
                     std::unique_ptr<RegexpList> regexps,
                     std::unique_ptr<RegexpList> exclusion_regexps,
                     bool skip_host_suffix_checking,
                     proto::BloomFilterFormat format);

  OptimizationFilter(const OptimizationFilter&) = delete;
  OptimizationFilter& operator=(const OptimizationFilter&) = delete;

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

  std::unique_ptr<BloomFilter> bloom_filter_;

  std::unique_ptr<RegexpList> regexps_;

  std::unique_ptr<RegexpList> exclusion_regexps_;

  bool skip_host_suffix_checking_ = false;

  proto::BloomFilterFormat bloom_filter_format_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_FILTER_H_
