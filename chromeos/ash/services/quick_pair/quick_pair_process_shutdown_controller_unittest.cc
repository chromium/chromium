// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process_shutdown_controller.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class QuickPairProcessShutdownControllerTest : public testing::Test {
 public:
  void SetUp() override {
    shutdown_controller_ =
        std::make_unique<QuickPairProcessShutdownController>();
  }

  void TearDown() override { shutdown_controller_.reset(); }

  void Start() {
    shutdown_controller_->Start(
        base::BindOnce(&QuickPairProcessShutdownControllerTest::OnStart,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnStart() { start_ = true; }

 protected:
  bool start_ = false;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<QuickPairProcessShutdownController> shutdown_controller_;
  base::WeakPtrFactory<QuickPairProcessShutdownControllerTest>
      weak_ptr_factory_{this};
};

TEST_F(QuickPairProcessShutdownControllerTest, Start) {
  EXPECT_FALSE(start_);
  Start();
  task_environment_.FastForwardBy(base::Seconds(5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(start_);
}

TEST_F(QuickPairProcessShutdownControllerTest, Stop) {
  EXPECT_FALSE(start_);
  Start();
  shutdown_controller_->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(start_);
}

}  // namespace quick_pair
}  // namespace ash
