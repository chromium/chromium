// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"

#include "base/metrics/metrics_hashes.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class DatabaseApiClientsTest : public DefaultModelTestBase {
 public:
  DatabaseApiClientsTest()
      : DefaultModelTestBase(std::make_unique<DatabaseApiClients>()) {}
  ~DatabaseApiClientsTest() override = default;
};

TEST_F(DatabaseApiClientsTest, VerifySignalsTracked) {
  ExpectInitAndFetchModel();
  const auto& sql_feature = fetched_metadata_->input_features(0).sql_feature();
  const auto& ukm_event = sql_feature.signal_filter().ukm_events(0);
  EXPECT_EQ(ukm_event.event_hash(), base::HashMetricName("TestEvents"));
  EXPECT_EQ(ukm_event.metric_hash_filter_size(), 3);
}

}  // namespace segmentation_platform
