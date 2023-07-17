// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TEST_TOP_BUCKET_DOMAINS_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TEST_TOP_BUCKET_DOMAINS_H_

#include <cstddef>

// This file is identical to top_bucket_domains.h except for the namespace. It's
// only used in browser tests.

namespace test_top_bucket_domains {

extern const char* const kTopBucketEditDistanceSkeletons[];
extern const size_t kNumTopBucketEditDistanceSkeletons;

extern const char* const kTopKeywords[];
extern const size_t kNumTopKeywords;

}  // namespace test_top_bucket_domains

#endif  //  COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TEST_TOP_BUCKET_DOMAINS_H_
