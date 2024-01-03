// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

class ModelQualityLogEntryTest : public testing::Test {};

// Test ModelQualityLogEntry initialization and logging_metadata().
TEST_F(ModelQualityLogEntryTest, Initialize) {
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
      new proto::LogAiDataRequest());
  optimization_guide::proto::LoggingMetadata* logging_metadata =
      log_ai_data_request.get()->mutable_logging_metadata();

  ModelQualityLogEntry log_entry(std::move(log_ai_data_request));

  EXPECT_EQ(log_entry.logging_metadata(), logging_metadata);
}

// Test client id is correctly set.
TEST_F(ModelQualityLogEntryTest, ClientId) {
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
      std::make_unique<proto::LogAiDataRequest>());
  int64_t client_id =
      log_ai_data_request.get()->mutable_logging_metadata()->client_id();

  ModelQualityLogEntry log_entry(std::move(log_ai_data_request));

  EXPECT_EQ(log_entry.client_id(), client_id);
}

}  // namespace optimization_guide
