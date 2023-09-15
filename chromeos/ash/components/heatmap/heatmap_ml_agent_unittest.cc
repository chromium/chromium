// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/heatmap/heatmap_ml_agent.h"

#include "base/run_loop.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using chromeos::machine_learning::mojom::ExecuteResult;
using chromeos::machine_learning::mojom::TensorPtr;

constexpr double kExpectedResult = 0.968;
constexpr int kExpectedDataLength = 3648;

class HeatmapMlAgentTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::MachineLearningClient::InitializeFake();
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
    fake_service_connection_.SetOutputValue(
        std::vector<int64_t>{1L, 1L}, std::vector<double>{kExpectedResult});
  }

  void TearDown() override { chromeos::MachineLearningClient::Shutdown(); }

 protected:
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(HeatmapMlAgentTest, RunsExecute) {
  HeatmapMlAgent agent;
  std::vector<double> data(kExpectedDataLength, 0);

  bool callback_done = false;
  agent.Execute(data,
                base::BindOnce(
                    [](bool* callback_done, absl::optional<double> result) {
                      EXPECT_TRUE(result.has_value());
                      EXPECT_EQ(result.value(), kExpectedResult);
                      *callback_done = true;
                    },
                    &callback_done));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(HeatmapMlAgentTest, ReturnsNulloptOnInvalidData) {
  HeatmapMlAgent agent;
  std::vector<double> data;

  bool callback_done = false;
  agent.Execute(data,
                base::BindOnce(
                    [](bool* callback_done, absl::optional<double> result) {
                      EXPECT_EQ(result, absl::nullopt);
                      *callback_done = true;
                    },
                    &callback_done));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

}  // namespace
}  // namespace ash
