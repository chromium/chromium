// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/footprints/public/drive_disclaimer_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/contextual_search/footprints/public/fpop_service.h"
#include "components/contextual_search/footprints/public/proto/footprints_oneplatform.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive_picker {
namespace {

class MockFpopService : public contextual_search::FpopService {
 public:
  MOCK_METHOD(
      void,
      GetFacs,
      (const footprints::oneplatform::GetFacsRequest& request,
       base::OnceCallback<void(
           bool success,
           const footprints::oneplatform::GetFacsResponse& response)> callback),
      (override));
  MOCK_METHOD(
      void,
      UpdateActivityControlsSettings,
      (const footprints::oneplatform::UpdateActivityControlsSettingsRequest&
           request,
       base::OnceCallback<void(
           bool success,
           const footprints::oneplatform::
               UpdateActivityControlsSettingsResponse& response)> callback),
      (override));
  MOCK_METHOD(
      void,
      ShouldShowMobileConsentFlow,
      (const footprints::oneplatform::ShouldShowMobileConsentFlowRequest&
           request,
       base::OnceCallback<void(
           bool success,
           const footprints::oneplatform::ShouldShowMobileConsentFlowResponse&
               response)> callback),
      (override));
};

class DriveDisclaimerControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_service = std::make_unique<MockFpopService>();
    mock_fpop_service_ = mock_service.get();
    controller_ =
        std::make_unique<DriveDisclaimerController>(std::move(mock_service));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DriveDisclaimerController> controller_;
  raw_ptr<MockFpopService> mock_fpop_service_;
};

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusAccepted) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, ShouldShowMobileConsentFlow(_, _))
      .WillOnce(
          [](const footprints::oneplatform::ShouldShowMobileConsentFlowRequest&
                 request,
             base::OnceCallback<void(
                 bool, const footprints::oneplatform::
                           ShouldShowMobileConsentFlowResponse&)> callback) {
            EXPECT_EQ(request.consent_flow(), 53);
            ASSERT_EQ(request.settings_size(), 1);
            EXPECT_EQ(request.settings(0).consent_id(), 38);

            footprints::oneplatform::ShouldShowMobileConsentFlowResponse
                response;
            response.mutable_should_show_flow_result()
                ->mutable_eligibility()
                ->set_status(3);  // ALREADY_CONSENTED
            std::move(callback).Run(true, response);
          });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kAccepted);
}

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusRestricted) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, ShouldShowMobileConsentFlow(_, _))
      .WillOnce(
          [](const footprints::oneplatform::ShouldShowMobileConsentFlowRequest&
                 request,
             base::OnceCallback<void(
                 bool, const footprints::oneplatform::
                           ShouldShowMobileConsentFlowResponse&)> callback) {
            EXPECT_EQ(request.consent_flow(), 53);

            footprints::oneplatform::ShouldShowMobileConsentFlowResponse
                response;
            response.mutable_should_show_flow_result()
                ->mutable_eligibility()
                ->set_status(2);  // CANNOT_CONSENT
            std::move(callback).Run(true, response);
          });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kRestricted);
}

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusFailure) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, ShouldShowMobileConsentFlow(_, _))
      .WillOnce(
          [](const footprints::oneplatform::ShouldShowMobileConsentFlowRequest&
                 request,
             base::OnceCallback<void(
                 bool, const footprints::oneplatform::
                           ShouldShowMobileConsentFlowResponse&)> callback) {
            footprints::oneplatform::ShouldShowMobileConsentFlowResponse
                response;
            std::move(callback).Run(false, response);
          });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  // Defaults to restricted on failure now
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kRestricted);
}

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusNoResultInResponse) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, ShouldShowMobileConsentFlow(_, _))
      .WillOnce(
          [](const footprints::oneplatform::ShouldShowMobileConsentFlowRequest&
                 request,
             base::OnceCallback<void(
                 bool, const footprints::oneplatform::
                           ShouldShowMobileConsentFlowResponse&)> callback) {
            footprints::oneplatform::ShouldShowMobileConsentFlowResponse
                response;
            // Success is true, but no should_show_flow_result is populated.
            std::move(callback).Run(true, response);
          });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kRestricted);
}

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusEligibleCanConsent) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, ShouldShowMobileConsentFlow(_, _))
      .WillOnce(
          [](const footprints::oneplatform::ShouldShowMobileConsentFlowRequest&
                 request,
             base::OnceCallback<void(
                 bool, const footprints::oneplatform::
                           ShouldShowMobileConsentFlowResponse&)> callback) {
            footprints::oneplatform::ShouldShowMobileConsentFlowResponse
                response;
            response.mutable_should_show_flow_result()
                ->mutable_eligibility()
                ->set_status(1);  // CAN_CONSENT
            std::move(callback).Run(true, response);
          });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kNotAccepted);
}

}  // namespace
}  // namespace drive_picker
