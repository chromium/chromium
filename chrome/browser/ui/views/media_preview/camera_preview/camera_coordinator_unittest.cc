// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "content/public/browser/video_capture_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

using testing::_;

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kDeviceId2[] = "device_id_2";
constexpr char kDeviceName2[] = "device_name_2";

}  // namespace

class CameraCoordinatorTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    content::OverrideVideoCaptureServiceForTesting(
        &fake_video_capture_service_);
    fake_video_capture_service_.SetOnGetVideoSourceCallback(
        mock_video_source_callback_.Get());
    parent_view_ = std::make_unique<views::View>();
    coordinator_ = std::make_unique<CameraCoordinator>(*parent_view_,
                                                       /*needs_borders=*/true);
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    coordinator_.reset();
    parent_view_.reset();
    content::OverrideVideoCaptureServiceForTesting(nullptr);
    TestWithBrowserView::TearDown();
  }

  const CameraSelectorComboboxModel& GetComboboxModel() const {
    return coordinator_->GetComboboxModelForTest();
  }

  void VerifyEmptyCombobox() const {
    // Our combobox model size will always be >= 1. If no cameras are connected,
    // a message is shown to the user to connect a camera.
    // Verify that there is precisely one item in the combobox model.
    EXPECT_EQ(GetComboboxModel().GetItemCount(), size_t(1));
    EXPECT_EQ(
        GetComboboxModel().GetItemAt(/*index=*/0),
        l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND_COMBOBOX));
  }

  base::SystemMonitor monitor_;
  std::unique_ptr<views::View> parent_view_;
  std::unique_ptr<CameraCoordinator> coordinator_;
  media_effects::FakeVideoCaptureService fake_video_capture_service_;
  base::MockCallback<
      media_effects::FakeVideoSourceProvider::GetVideoSourceCallback>
      mock_video_source_callback_;
};

TEST_F(CameraCoordinatorTest, RelevantVideoCaptureDeviceInfoExtraction) {
  VerifyEmptyCombobox();

  // Add first camera, and connect to it.
  // Camera connection is done automatically to the device at combobox's default
  // index (i.e. 0).
  base::RunLoop run_loop;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId, _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  fake_video_capture_service_.AddFakeCamera({kDeviceName, kDeviceId});
  run_loop.Run();
  EXPECT_EQ(GetComboboxModel().GetItemCount(), size_t(1));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0),
            base::UTF8ToUTF16({kDeviceName}));

  // Add second camera and connection to the first is not affected.
  base::RunLoop run_loop2;
  EXPECT_CALL(mock_video_source_callback_, Run).Times(0);
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      run_loop2.QuitClosure());
  fake_video_capture_service_.AddFakeCamera({kDeviceName2, kDeviceId2});
  run_loop2.Run();
  EXPECT_EQ(GetComboboxModel().GetItemCount(), size_t(2));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0),
            base::UTF8ToUTF16({kDeviceName}));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/1),
            base::UTF8ToUTF16({kDeviceName2}));

  // Remove first camera, and connect to the second one.
  base::RunLoop run_loop3;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId2, _))
      .WillOnce(base::test::RunOnceClosure(run_loop3.QuitClosure()));
  fake_video_capture_service_.RemoveFakeCamera(kDeviceId);
  run_loop3.Run();
  EXPECT_EQ(GetComboboxModel().GetItemCount(), size_t(1));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0),
            base::UTF8ToUTF16({kDeviceName2}));

  // Re-add first camera and connect to it.
  base::RunLoop run_loop4;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId, _))
      .WillOnce(base::test::RunOnceClosure(run_loop4.QuitClosure()));
  fake_video_capture_service_.AddFakeCamera({kDeviceName, kDeviceId});
  run_loop4.Run();
  EXPECT_EQ(GetComboboxModel().GetItemCount(), size_t(2));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0),
            base::UTF8ToUTF16({kDeviceName}));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/1),
            base::UTF8ToUTF16({kDeviceName2}));

  // Remove second camera, and connection to the first is not affected.
  base::RunLoop run_loop5;
  EXPECT_CALL(mock_video_source_callback_, Run).Times(0);
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      run_loop5.QuitClosure());
  fake_video_capture_service_.RemoveFakeCamera(kDeviceId2);
  run_loop5.Run();
  EXPECT_EQ(GetComboboxModel().GetItemCount(), size_t(1));
  EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0),
            base::UTF8ToUTF16({kDeviceName}));

  // Remove first camera.
  base::RunLoop run_loop6;
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      run_loop6.QuitClosure());
  fake_video_capture_service_.RemoveFakeCamera(kDeviceId);
  run_loop6.Run();
  VerifyEmptyCombobox();
}

TEST_F(CameraCoordinatorTest, ConnectToDifferentDevice) {
  VerifyEmptyCombobox();

  // Add first camera, and connect to it.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId, _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  fake_video_capture_service_.AddFakeCamera({kDeviceName, kDeviceId});
  run_loop.Run();

  // Add second camera and connection to the first is not affected.
  base::RunLoop run_loop2;
  EXPECT_CALL(mock_video_source_callback_, Run).Times(0);
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      run_loop2.QuitClosure());
  fake_video_capture_service_.AddFakeCamera({kDeviceName2, kDeviceId2});
  run_loop2.Run();

  //  Connect to the second camera.
  base::RunLoop run_loop3;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId2, _))
      .WillOnce(base::test::RunOnceClosure(run_loop3.QuitClosure()));
  coordinator_->OnVideoSourceChanged(/*selected_index=*/1);
  run_loop3.Run();
}

TEST_F(CameraCoordinatorTest, TryConnectToSameDevice) {
  VerifyEmptyCombobox();

  // Add camera, and connect to it.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId, _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  fake_video_capture_service_.AddFakeCamera({kDeviceName, kDeviceId});
  run_loop.Run();

  //  Try connect to the camera.
  // Nothing is expected because we are already connected.
  EXPECT_CALL(mock_video_source_callback_, Run).Times(0);
  coordinator_->OnVideoSourceChanged(/*selected_index=*/0);
  base::RunLoop().RunUntilIdle();

  // Remove camera.
  base::RunLoop run_loop2;
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      run_loop2.QuitClosure());
  fake_video_capture_service_.RemoveFakeCamera(kDeviceId);
  run_loop2.Run();
  VerifyEmptyCombobox();

  // Add camera, and connect to it again.
  base::RunLoop run_loop3;
  EXPECT_CALL(mock_video_source_callback_, Run(kDeviceId, _))
      .WillOnce(base::test::RunOnceClosure(run_loop3.QuitClosure()));
  fake_video_capture_service_.AddFakeCamera({kDeviceName, kDeviceId});
  run_loop3.Run();
}
