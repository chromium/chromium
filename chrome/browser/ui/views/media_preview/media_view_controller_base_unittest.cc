// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"

class MediaViewControllerBaseTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    media_view_ = std::make_unique<MediaView>(/*is_subsection=*/false);
    controller_ = std::make_unique<MediaViewControllerBase>(
        *media_view_, /*needs_borders=*/true, /*model=*/nullptr,
        combobox_selection_change_callback_.Get(), std::u16string(),
        std::u16string());
  }

  void TearDown() override {
    controller_.reset();
    media_view_.reset();
    TestWithBrowserView::TearDown();
  }

  bool IsComboboxEnabled() const {
    return controller_->device_selector_combobox_->GetEnabled();
  }
  bool IsNoDeviceLabelVisible() const {
    return controller_->no_device_connected_label_->GetVisible();
  }

  void VerifyActiveDeviceID(const std::string& device_id) const {
    EXPECT_EQ(controller_->active_device_id_, device_id);
  }

  std::unique_ptr<MediaView> media_view_;
  base::MockCallback<base::RepeatingClosure>
      combobox_selection_change_callback_;
  std::unique_ptr<MediaViewControllerBase> controller_;
};

TEST_F(MediaViewControllerBaseTest, UpdateComboboxEnabledStateTest) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_FALSE(IsComboboxEnabled());

  EXPECT_CALL(combobox_selection_change_callback_, Run());
  controller_->AdjustComboboxEnabledState(/*has_devices=*/true);

  EXPECT_FALSE(IsNoDeviceLabelVisible());
  EXPECT_TRUE(IsComboboxEnabled());
}

TEST_F(MediaViewControllerBaseTest, UpdateActiveDeviceIdTest) {
  VerifyActiveDeviceID(std::string());

  constexpr char kActiveDeviceID[] = "device_id";

  EXPECT_TRUE(controller_->UpdateActiveDeviceId(kActiveDeviceID));
  VerifyActiveDeviceID(kActiveDeviceID);

  EXPECT_FALSE(controller_->UpdateActiveDeviceId(kActiveDeviceID));
  VerifyActiveDeviceID(kActiveDeviceID);
}
