// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_view_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/grit/generated_resources.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;

class CameraViewControllerTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    media_view_ = std::make_unique<MediaView>(/*is_subsection=*/false);
    controller_ = std::make_unique<CameraViewController>(
        *media_view_, source_change_callback_.Get(),
        /*needs_borders=*/true, combobox_model_);
  }

  void TearDown() override {
    controller_.reset();
    media_view_.reset();
    TestWithBrowserView::TearDown();
  }

  void VerifyComboboxModelItemCount(size_t size) const {
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(combobox_model_.GetItemCount(), size));
  }

  void VerifyComboboxModelItemAt(int index,
                                 const std::u16string& device_id) const {
    EXPECT_EQ(combobox_model_.GetItemAt(index), device_id);
  }

  void VerifyActiveDeviceId(const std::string& device_id) const {
    EXPECT_FALSE(
        controller_->base_controller_->UpdateActiveDeviceId(device_id));
  }

  std::unique_ptr<MediaView> media_view_;
  base::MockCallback<CameraViewController::SourceChangeCallback>
      source_change_callback_;
  CameraSelectorComboboxModel combobox_model_;
  std::unique_ptr<CameraViewController> controller_;
};

TEST_F(CameraViewControllerTest, UpdateDeviceListAndSourceChanged) {
  const int kIndex = 2;  // any random value
  const std::vector<VideoSourceInfo> kVideoSourceInfos = {
      {"id_0", u"name_0", media::VideoCaptureFormats()},
      {"id_1", u"name_1", media::VideoCaptureFormats()},
      {"id_2", u"name_2", media::VideoCaptureFormats()},
  };

  // Our combobox model size will always be >= 1. If no cameras are connected, a
  // message is shown to the user to connect a camera.
  // Verify that there is precisely one item in the combobox model.
  VerifyComboboxModelItemCount(/*size=*/1);
  VerifyComboboxModelItemAt(
      /*index=*/0,
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND_COMBOBOX));
  VerifyActiveDeviceId(std::string());

  EXPECT_CALL(
      source_change_callback_,
      Run(kVideoSourceInfos[combobox_model_.GetDefaultIndex().value_or(0)]));
  controller_->UpdateVideoSourceInfos(kVideoSourceInfos);

  VerifyComboboxModelItemCount(/*size=*/kVideoSourceInfos.size());
  VerifyComboboxModelItemAt(kIndex, kVideoSourceInfos[kIndex].name_and_model);
  VerifyActiveDeviceId(kVideoSourceInfos[0].id);
}
