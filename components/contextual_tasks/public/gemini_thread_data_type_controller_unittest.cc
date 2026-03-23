// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/gemini_thread_data_type_controller.h"

#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace contextual_tasks {

class GeminiThreadDataTypeControllerTest : public testing::Test {
 public:
  GeminiThreadDataTypeControllerTest() = default;
  ~GeminiThreadDataTypeControllerTest() override = default;

  void SetUp() override {
    contextual_tasks_service_ =
        std::make_unique<NiceMock<MockContextualTasksService>>();

    controller_ = std::make_unique<GeminiThreadDataTypeController>(
        contextual_tasks_service_.get(),
        std::make_unique<syncer::FakeDataTypeControllerDelegate>(
            syncer::GEMINI_THREAD),
        std::make_unique<syncer::FakeDataTypeControllerDelegate>(
            syncer::GEMINI_THREAD));
  }

 protected:
  std::unique_ptr<MockContextualTasksService> contextual_tasks_service_;
  std::unique_ptr<GeminiThreadDataTypeController> controller_;
};

TEST_F(GeminiThreadDataTypeControllerTest, PreconditionsMetWhenEligible) {
  ON_CALL(*contextual_tasks_service_, IsGeminiThreadsEligible)
      .WillByDefault(testing::Return(true));
  EXPECT_EQ(
      syncer::DataTypeController::PreconditionState::kPreconditionsMet,
      controller_->GetPreconditionState(
          syncer::DataTypeController::PreconditionContext(
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail)));
}

TEST_F(GeminiThreadDataTypeControllerTest, StopAndKeepDataWhenIneligible) {
  ON_CALL(*contextual_tasks_service_, IsGeminiThreadsEligible)
      .WillByDefault(testing::Return(false));
  EXPECT_EQ(
      syncer::DataTypeController::PreconditionState::kMustStopAndKeepData,
      controller_->GetPreconditionState(
          syncer::DataTypeController::PreconditionContext(
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail)));
}

}  // namespace contextual_tasks
