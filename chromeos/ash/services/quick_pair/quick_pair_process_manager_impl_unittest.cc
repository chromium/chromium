// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include <memory>

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_shutdown_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeProcessShutdownController
    : public QuickPairProcessShutdownController {
 public:
  void Start(base::OnceClosure callback) override {
    is_running_ = true;
    callback_ = std::move(callback);
  }

  void Stop() override { is_running_ = false; }

  bool is_running() { return is_running_; }

  void FireCallback() { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
  bool is_running_;
};

class QuickPairProcessManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<QuickPairProcessShutdownController> shutdown_controller =
        std::make_unique<FakeProcessShutdownController>();

    shutdown_controller_ =
        static_cast<FakeProcessShutdownController*>(shutdown_controller.get());

    process_manager_ = std::make_unique<QuickPairProcessManagerImpl>(
        std::move(shutdown_controller));

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
  }

 protected:
  base::test::TaskEnvironment task_enviornment_;
  raw_ptr<FakeProcessShutdownController, DanglingUntriaged>
      shutdown_controller_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
};

TEST_F(QuickPairProcessManagerImplTest, ProcessStartedWhenReferencesRequested) {
  EXPECT_CALL(*(browser_delegate_.get()), RequestService);
  process_manager_->GetProcessReference(base::DoNothing());
}

TEST_F(QuickPairProcessManagerImplTest,
       ProcessStartedOnceWhenMultipleReferencesRequested) {
  EXPECT_CALL(*(browser_delegate_.get()), RequestService);
  process_manager_->GetProcessReference(base::DoNothing());
  process_manager_->GetProcessReference(base::DoNothing());
}

TEST_F(QuickPairProcessManagerImplTest,
       ShutdownStartedWhenNoActiveReferencesRemaining) {
  EXPECT_CALL(*(browser_delegate_.get()), RequestService);
  auto reference = process_manager_->GetProcessReference(base::DoNothing());
  EXPECT_FALSE(shutdown_controller_->is_running());

  reference.reset();

  EXPECT_TRUE(shutdown_controller_->is_running());
}

TEST_F(QuickPairProcessManagerImplTest,
       DeletedReferencesArentNotifiedOnProcessStop) {
  EXPECT_CALL(*(browser_delegate_.get()), RequestService);
  base::MockCallback<
      base::OnceCallback<void(QuickPairProcessManager::ShutdownReason)>>
      callback;

  auto reference = process_manager_->GetProcessReference(callback.Get());
  reference.reset();

  EXPECT_CALL(callback, Run(QuickPairProcessManager::ShutdownReason::kNormal))
      .Times(0);
  shutdown_controller_->FireCallback();
}

}  // namespace quick_pair
}  // namespace ash
