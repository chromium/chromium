// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_TEST_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_TEST_UTILS_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {
class FeatureMap;

// Mock implementation of Scorer used to intercept and verify calls to the
// visual TfLite models without executing the real model logic.
class MockScorer : public Scorer {
 public:
  MockScorer();
  ~MockScorer() override;

  MOCK_METHOD(void,
              ApplyVisualTfLiteModel,
              (const gfx::Image& image,
               base::OnceCallback<void(std::vector<double>)> callback),
              (const, override));

  MOCK_METHOD(void,
              ApplyVisualTfLiteModelImageEmbedding,
              (const gfx::Image& image,
               base::OnceCallback<void(ImageFeatureEmbedding)> callback),
              (const, override));
};

// Compares two FeatureMap objects using gMock.  Always use this instead of
// operator== or ContainerEq, since hash_map's equality operator may return
// false if the elements were inserted in different orders.
void ExpectFeatureMapsAreEqual(const FeatureMap& first,
                               const FeatureMap& second);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_TEST_UTILS_H_
