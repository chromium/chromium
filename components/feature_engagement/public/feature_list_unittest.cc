// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_list.h"

#include <set>

#include "base/containers/contains.h"
#include "base/strings/levenshtein_distance.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

std::string FindClosest(const std::string& str,
                        const std::set<std::string>& candidates) {
  std::string result;
  std::optional<size_t> dist;
  for (const std::string& candidate : candidates) {
    const size_t cur_dist = base::LevenshteinDistance(str, candidate, dist);
    if (!dist || cur_dist < dist) {
      dist = cur_dist;
      result = candidate;
    }
  }
  return result;
}

}  // namespace

TEST(FeatureListTest, ConsistencyCheck) {
  std::set<std::string> known_feature_names;
  for (const auto* feature : GetAllFeatures()) {
    known_feature_names.emplace(feature->name);
  }

  std::map<std::string, std::string> errors;

  for (const auto& entry : kIPHDemoModeChoiceVariations) {
    const char* const given_name = entry.params[0].param_value;
    if (!base::Contains(known_feature_names, given_name)) {
      errors.emplace(given_name, FindClosest(given_name, known_feature_names));
    }
  }

  if (!errors.empty()) {
    LOG(ERROR)
        << "Inconsistencies found between DEFINE_VARIATION_PARAM() entries in "
           "components/feature_engagement/public/feature_list.h and "
           "kAllFeatures in "
           "components/feature_engagement/public/feature_list.cc:";
    for (const auto& [entry, closest] : errors) {
      LOG(ERROR) << "Found variation " << entry << " - did you mean " << closest
                 << "?";
    }
    GTEST_FAIL();
  }
}

}  // namespace feature_engagement
