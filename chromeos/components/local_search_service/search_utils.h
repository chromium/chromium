// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_UTILS_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_UTILS_H_

#include <string>

namespace chromeos {
namespace local_search_service {

struct Result;

// Score is non-zero if |query| is a prefix of |text|. No case normalization is
// done.
float ExactPrefixMatchScore(const std::u16string& query,
                            const std::u16string& text);

// Returns block matching ratio between |query| and |text|. No case
// normalization is done.
float BlockMatchScore(const std::u16string& query, const std::u16string& text);

// If the query's prefix score and block matching score are both below the
// corresponding thresholds, then this returns zero. Otherwise, returns the
// higher of the two scores, which will be in [0,1].
float RelevanceCoefficient(const std::u16string& query,
                           const std::u16string& text,
                           float prefix_threshold,
                           float block_threshold);

// Returns whether |r1| score is higher than |r2|'s.
bool CompareResults(const Result& r1, const Result& r2);

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_UTILS_H_
