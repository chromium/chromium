// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/onep_resolver_impl.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class OnePResolverImplTest : public ::testing::Test {
 public:
  OnePResolverImplTest() = default;
  ~OnePResolverImplTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  OnePResolverImpl resolver_;
};

// Verifies that RetrieveAll successfully executes its asynchronous callback,
// but returns an empty result set when the OneP resolver feature is disabled.
TEST_F(OnePResolverImplTest, FeatureDisabledReturnsEmpty) {
  scoped_feature_list_.InitAndDisableFeature(
      kAccessibilityAnnotationReducerOnePResolver);

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_.RetrieveAll(u"any query", future.GetCallback());

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that RetrieveAll successfully executes its asynchronous callback
// and, while the OneP resolver feature is still unimplemented, returns an empty
// result.
// TODO(b:487416734) Once fully implemented, this should not return empty
// results.
TEST_F(OnePResolverImplTest, FeatureEnabledReturnsEmpty) {
  scoped_feature_list_.InitAndEnableFeature(
      kAccessibilityAnnotationReducerOnePResolver);

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_.RetrieveAll(u"any query", future.GetCallback());

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

}  // namespace accessibility_annotator
