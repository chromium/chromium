// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/test_utils.h"

#include <map>
#include <string>

#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

void ExpectFeatureMapsAreEqual(const FeatureMap& first,
                               const FeatureMap& second) {
  std::map<std::string, double> sorted_first(first.features().begin(),
                                             first.features().end());
  std::map<std::string, double> sorted_second(second.features().begin(),
                                              second.features().end());
  EXPECT_THAT(sorted_first, testing::ContainerEq(sorted_second));
}

}  // namespace safe_browsing
