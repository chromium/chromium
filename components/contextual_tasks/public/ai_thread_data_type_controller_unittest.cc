// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/ai_thread_data_type_controller.h"

#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class AIThreadDataTypeControllerTest : public testing::Test {
 public:
  AIThreadDataTypeControllerTest() = default;
  ~AIThreadDataTypeControllerTest() override = default;

  void SetUp() override {
    AimEligibilityService::RegisterProfilePrefs(pref_service_.registry());
    mock_aim_eligibility_service_ = std::make_unique<MockAimEligibilityService>(
        pref_service_, nullptr, nullptr, nullptr,
        AimEligibilityService::Configuration{});
    controller_ = std::make_unique<AIThreadDataTypeController>(
        mock_aim_eligibility_service_.get(),
        std::make_unique<syncer::FakeDataTypeControllerDelegate>(
            syncer::AI_THREAD),
        std::make_unique<syncer::FakeDataTypeControllerDelegate>(
            syncer::AI_THREAD));
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
  std::unique_ptr<AIThreadDataTypeController> controller_;
};

TEST_F(AIThreadDataTypeControllerTest, PreconditionsMetWhenIneligible) {
  ON_CALL(*mock_aim_eligibility_service_, IsAimEligible)
      .WillByDefault(testing::Return(true));
  EXPECT_EQ(
      syncer::DataTypeController::PreconditionState::kPreconditionsMet,
      controller_->GetPreconditionState(
          syncer::DataTypeController::PreconditionContext(
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail)));
}

TEST_F(AIThreadDataTypeControllerTest, StopAndKeepDataWhenIneligible) {
  ON_CALL(*mock_aim_eligibility_service_, IsAimEligible)
      .WillByDefault(testing::Return(false));
  EXPECT_EQ(
      syncer::DataTypeController::PreconditionState::kMustStopAndKeepData,
      controller_->GetPreconditionState(
          syncer::DataTypeController::PreconditionContext(
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail)));
}

}  // namespace contextual_tasks
