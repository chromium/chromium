// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "content/public/browser/video_capture_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

using testing::_;
using testing::Pointwise;

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kDeviceId2[] = "device_id_2";
constexpr char kDeviceName2[] = "device_name_2";

MATCHER_P(HasItems, items, "") {
  if (arg.GetItemCount() != items.size()) {
    *result_listener << "item count is " << arg.GetItemCount();
    return false;
  }

  for (size_t i = 0; i < items.size(); ++i) {
    if (base::UTF8ToUTF16(items[i]) != arg.GetItemAt(i)) {
      *result_listener << "item at index " << i << " is " << arg.GetItemAt(i);
      return false;
    }
  }

  return true;
}

MATCHER(VideoCaptureDeviceInfoEq, "") {
  return std::get<0>(arg).descriptor.device_id ==
         std::get<1>(arg).descriptor.device_id;
}

}  // namespace

class CameraCoordinatorTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    content::OverrideVideoCaptureServiceForTesting(
        &fake_video_capture_service_);
    fake_video_capture_service_.SetOnGetVideoSourceCallback(
        on_get_video_source_future_.GetRepeatingCallback());

    parent_view_.emplace();
    InitializeCoordinator(/*eligible_camera_ids=*/{});
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    content::OverrideVideoCaptureServiceForTesting(nullptr);
    TestWithBrowserView::TearDown();
  }

  void InitializeCoordinator(std::vector<std::string> eligible_camera_ids) {
    CHECK(profile()->GetPrefs());
    coordinator_.emplace(
        *parent_view_,
        /*needs_borders=*/true, eligible_camera_ids, *profile()->GetPrefs(),
        /*allow_device_selection=*/true,
        media_preview_metrics::Context(
            media_preview_metrics::UiLocation::kPermissionPrompt));
  }

  const ui::SimpleComboboxModel& GetComboboxModel() const {
    return coordinator_->GetComboboxModelForTest();
  }

  void VerifyEmptyCombobox() const {
    // Our combobox model size will always be >= 1.
    // Verify that there is precisely one item in the combobox model.
    EXPECT_EQ(GetComboboxModel().GetItemCount(), 1u);
    EXPECT_EQ(GetComboboxModel().GetItemAt(/*index=*/0), std::u16string());
  }

  bool AddFakeCamera(const media::VideoCaptureDeviceDescriptor& descriptor) {
    replied_with_source_infos_future_.Clear();
    fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
        replied_with_source_infos_future_.GetCallback());
    fake_video_capture_service_.AddFakeCamera(descriptor);
    return replied_with_source_infos_future_.WaitAndClear();
  }

  bool RemoveFakeCamera(const std::string& device_id) {
    replied_with_source_infos_future_.Clear();
    fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
        replied_with_source_infos_future_.GetCallback());
    fake_video_capture_service_.RemoveFakeCamera(device_id);
    return replied_with_source_infos_future_.WaitAndClear();
  }

  base::SystemMonitor monitor_;
  std::optional<views::View> parent_view_;
  std::optional<CameraCoordinator> coordinator_;
  media_effects::FakeVideoCaptureService fake_video_capture_service_;
  base::test::TestFuture<void> replied_with_source_infos_future_;
  base::test::TestFuture<
      const std::string&,
      mojo::PendingReceiver<video_capture::mojom::VideoSource>>
      on_get_video_source_future_;
};

TEST_F(CameraCoordinatorTest, RelevantVideoCaptureDeviceInfoExtraction) {
  VerifyEmptyCombobox();

  // Add first camera, and connect to it.
  // camera connection is done automatically to the device at combobox's default
  // index (i.e. 0).
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName}));

  // Add second camera and connection to the first is not affected.
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));

  // Remove first camera, and connect to the second one.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Re-add first camera and connect to it.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));

  // Remove second camera, and connection to the first is not affected.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId2));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName}));

  // Remove first camera.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  VerifyEmptyCombobox();
}

TEST_F(CameraCoordinatorTest,
       RelevantVideoCaptureDeviceInfoExtraction_ConstrainedToEligibleDevices) {
  InitializeCoordinator({kDeviceId2});
  VerifyEmptyCombobox();

  // Add first camera. It won't be added to the combobox because it's not in the
  // eligible list.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  VerifyEmptyCombobox();

  // Add second camera and connect to it since it's in the eligible list.
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Remove first camera, nothing changes since it wasn't in the combobox.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Remove second camera.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId2));
  VerifyEmptyCombobox();
}

TEST_F(CameraCoordinatorTest, ConnectToDifferentDevice) {
  VerifyEmptyCombobox();

  // Add first camera, and connect to it.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);

  // Add second camera and connection to the first is not affected.
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());

  //  Connect to the second camera.
  coordinator_->OnVideoSourceChanged(/*selected_index=*/1);
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);
}

TEST_F(CameraCoordinatorTest, TryConnectToSameDevice) {
  VerifyEmptyCombobox();

  // Add camera, and connect to it.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);

  //  Try connect to the camera.
  // Nothing is expected because we are already connected.
  coordinator_->OnVideoSourceChanged(/*selected_index=*/0);
  EXPECT_FALSE(on_get_video_source_future_.IsReady());

  // Remove camera.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  VerifyEmptyCombobox();

  // Add camera, and connect to it again.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
}

TEST_F(CameraCoordinatorTest, UpdateDevicePreferenceRanking) {
  VerifyEmptyCombobox();
  const media::VideoCaptureDeviceInfo kDevice1{{kDeviceName, kDeviceId}};
  const media::VideoCaptureDeviceInfo kDevice2{{kDeviceName2, kDeviceId2}};

  ASSERT_TRUE(AddFakeCamera(kDevice1.descriptor));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  ASSERT_TRUE(AddFakeCamera(kDevice2.descriptor));
  on_get_video_source_future_.Clear();

  // Select device 2.
  coordinator_->OnVideoSourceChanged(/*selected_index=*/1);
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);

  // Preference ranking defaults to noop.
  std::vector device_infos{kDevice1, kDevice2};
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              device_infos);
  EXPECT_THAT(device_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kDevice1, kDevice2}));

  // Ranking is updated to make the currently selected device most preferred.
  coordinator_->UpdateDevicePreferenceRanking();
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              device_infos);
  EXPECT_THAT(device_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kDevice2, kDevice1}));
}
