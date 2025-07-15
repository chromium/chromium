// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/detected_agent_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

class DetectedAgentClientTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 protected:
  DetectedAgentClientTest() {
    scoped_feature_list_.InitWithFeatureState(
        enterprise_signals::features::kDetectedAgentSignalCollectionEnabled,
        is_detected_agent_signal_collection_enabled());
  }

  bool is_detected_agent_signal_collection_enabled() { return GetParam(); }

  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath(bool is_valid) {
    return is_valid ? scoped_temp_dir_.GetPath()
                    : base::FilePath(FILE_PATH_LITERAL("test/"));
  }

  void CreateClient(bool is_valid) {
    client_ = DetectedAgentClient::Create();
    DetectedAgentClient::SetFilePathForTesting(GetPath(is_valid));
  }

  void ValidateSignal(std::vector<Agents> agents) {
    if (is_detected_agent_signal_collection_enabled()) {
      EXPECT_TRUE(agents.size() == 1);
      EXPECT_EQ(agents[0], Agents::kCrowdStrikeFalcon);
    } else {
      EXPECT_TRUE(agents.empty());
    }
  }

  std::vector<Agents> GetSignal() {
    base::test::TestFuture<std::vector<Agents>> future;

    client_->GetAgents(future.GetCallback());

    return future.Get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir scoped_temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DetectedAgentClient> client_;
};

TEST_P(DetectedAgentClientTest, Agents_InvalidPath) {
  CreateClient(/*is_valid=*/false);

  EXPECT_TRUE(GetSignal().empty());
}

TEST_P(DetectedAgentClientTest, Agents_Success) {
  CreateClient(/*is_valid=*/true);
  ValidateSignal(GetSignal());
}

INSTANTIATE_TEST_SUITE_P(, DetectedAgentClientTest, testing::Bool());

}  // namespace device_signals
