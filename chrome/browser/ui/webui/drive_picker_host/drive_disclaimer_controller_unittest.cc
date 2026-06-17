// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_disclaimer_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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

  EXPECT_CALL(*mock_fpop_service_, GetFacs(_, _))
      .WillOnce([](const footprints::oneplatform::GetFacsRequest& request,
                   base::OnceCallback<void(
                       bool, const footprints::oneplatform::GetFacsResponse&)>
                       callback) {
        EXPECT_EQ(request.header().application_id(),
                  "chrome_desktop_disclaimer");
        ASSERT_EQ(request.setting_size(), 1);
        EXPECT_EQ(request.setting(0),
                  contextual_search::kContextualSearchDriveDisclaimerAccepted);

        footprints::oneplatform::GetFacsResponse response;
        auto* setting = response.add_facs_setting();
        setting->set_setting(
            contextual_search::kContextualSearchDriveDisclaimerAccepted);
        setting->set_data_recording_enabled(true);
        std::move(callback).Run(true, response);
      });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kAccepted);
}

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusRestricted) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, GetFacs(_, _))
      .WillOnce([](const footprints::oneplatform::GetFacsRequest& request,
                   base::OnceCallback<void(
                       bool, const footprints::oneplatform::GetFacsResponse&)>
                       callback) {
        EXPECT_EQ(request.header().application_id(),
                  "chrome_desktop_disclaimer");
        ASSERT_EQ(request.setting_size(), 1);
        EXPECT_EQ(request.setting(0),
                  contextual_search::kContextualSearchDriveDisclaimerAccepted);

        footprints::oneplatform::GetFacsResponse response;
        auto* setting = response.add_facs_setting();
        setting->set_setting(
            contextual_search::kContextualSearchDriveDisclaimerAccepted);
        setting->mutable_recording_setting_info()
            ->add_user_setting_restricted_reason(
                footprints::oneplatform::UserSettingRestrictedReason::
                    PARENT_CONTROL);
        std::move(callback).Run(true, response);
      });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kRestricted);
}

TEST_F(DriveDisclaimerControllerTest, CheckDisclaimerStatusGetFacsFailure) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, GetFacs(_, _))
      .WillOnce([](const footprints::oneplatform::GetFacsRequest& request,
                   base::OnceCallback<void(
                       bool, const footprints::oneplatform::GetFacsResponse&)>
                       callback) {
        EXPECT_EQ(request.header().application_id(),
                  "chrome_desktop_disclaimer");
        ASSERT_EQ(request.setting_size(), 1);
        EXPECT_EQ(request.setting(0),
                  contextual_search::kContextualSearchDriveDisclaimerAccepted);

        footprints::oneplatform::GetFacsResponse response;
        std::move(callback).Run(false, response);
      });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kNotAccepted);
}

TEST_F(DriveDisclaimerControllerTest,
       CheckDisclaimerStatusNoSettingsInResponse) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, GetFacs(_, _))
      .WillOnce([](const footprints::oneplatform::GetFacsRequest& request,
                   base::OnceCallback<void(
                       bool, const footprints::oneplatform::GetFacsResponse&)>
                       callback) {
        EXPECT_EQ(request.header().application_id(),
                  "chrome_desktop_disclaimer");
        ASSERT_EQ(request.setting_size(), 1);
        EXPECT_EQ(request.setting(0),
                  contextual_search::kContextualSearchDriveDisclaimerAccepted);

        footprints::oneplatform::GetFacsResponse response;
        // Success is true, but no settings are populated.
        std::move(callback).Run(true, response);
      });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kNotAccepted);
}

TEST_F(DriveDisclaimerControllerTest,
       CheckDisclaimerStatusWrongSettingInResponse) {
  using ::testing::_;

  EXPECT_CALL(*mock_fpop_service_, GetFacs(_, _))
      .WillOnce([](const footprints::oneplatform::GetFacsRequest& request,
                   base::OnceCallback<void(
                       bool, const footprints::oneplatform::GetFacsResponse&)>
                       callback) {
        EXPECT_EQ(request.header().application_id(),
                  "chrome_desktop_disclaimer");
        ASSERT_EQ(request.setting_size(), 1);
        EXPECT_EQ(request.setting(0),

                  contextual_search::kContextualSearchDriveDisclaimerAccepted);

        footprints::oneplatform::GetFacsResponse response;
        auto* setting = response.add_facs_setting();
        // Populate response with a different/unrelated setting.
        setting->set_setting(999);
        setting->set_data_recording_enabled(true);
        std::move(callback).Run(true, response);
      });

  base::test::TestFuture<DriveDisclaimerController::DisclaimerStatus> future;
  controller_->CheckDisclaimerStatusAsync(future.GetCallback());
  EXPECT_EQ(future.Get(),
            DriveDisclaimerController::DisclaimerStatus::kNotAccepted);
}

}  // namespace
}  // namespace drive_picker
