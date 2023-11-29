// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

#include <memory>
#include <optional>
#include <string>

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
    media_view_ = std::make_unique<MediaView>();
    controller_ = std::make_unique<MediaViewControllerBase>(
        *media_view_, /*needs_borders=*/true, /*model=*/nullptr,
        source_change_callback_.Get(), std::u16string(), std::u16string());
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

  std::unique_ptr<MediaView> media_view_;
  base::MockCallback<MediaViewControllerBase::SourceChangeCallback>
      source_change_callback_;
  std::unique_ptr<MediaViewControllerBase> controller_;
};

TEST_F(MediaViewControllerBaseTest, UpdateComboboxEnabledStateTest) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_FALSE(IsComboboxEnabled());

  EXPECT_CALL(source_change_callback_, Run(testing::_))
      .WillOnce(
          [](absl::optional<size_t> index) { EXPECT_EQ(std::nullopt, index); });
  controller_->AdjustComboboxEnabledState(/*has_devices=*/true);

  EXPECT_FALSE(IsNoDeviceLabelVisible());
  EXPECT_TRUE(IsComboboxEnabled());
}
