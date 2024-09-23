// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/metrics_clustering.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class MetricsClusteringTest : public DefaultModelTestBase {
 public:
  MetricsClusteringTest()
      : DefaultModelTestBase(std::make_unique<MetricsClustering>()) {}
  ~MetricsClusteringTest() override = default;
};

TEST_F(MetricsClusteringTest, InitAndFetch) {
  ExpectInitAndFetchModel();
}

TEST_F(MetricsClusteringTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/false,
                           /*expected_result=*/{-1, -1, -1});
}

}  // namespace segmentation_platform
