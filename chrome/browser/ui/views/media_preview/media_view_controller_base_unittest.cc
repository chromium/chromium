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
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"

using testing::_;
using testing::Eq;

namespace {

constexpr char16_t kNoDeviceComboboxText[] = u"no_device_combobox_text";
constexpr char16_t kNoDeviceLabelText[] = u"no_device_label_text";
constexpr char16_t kComboboxAccessibleName[] = u"combobox_accessible_name";

std::u16string GetDeviceName(size_t index) {
  return base::UTF8ToUTF16(base::StringPrintf("device_%zu", index));
}

}  // namespace

class MediaViewControllerBaseTest : public TestWithBrowserView,
                                    public ui::ComboboxModelObserver {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    media_view_ = std::make_unique<MediaView>();
    combobox_model_ = std::make_unique<ui::SimpleComboboxModel>(
        std::vector<ui::SimpleComboboxModel::Item>());
    UpdateComboboxModel(0);
    controller_ = std::make_unique<MediaViewControllerBase>(
        *media_view_, /*needs_borders=*/true, combobox_model_.get(),
        source_change_callback_.Get(),
        /*combobox_accessible_name=*/kComboboxAccessibleName,
        /*no_devices_found_combobox_text=*/kNoDeviceComboboxText,
        /*no_devices_found_label_text=*/kNoDeviceLabelText,
        media_preview_metrics::Context(
            media_preview_metrics::UiLocation::kPermissionPrompt));
    combobox_model_->AddObserver(this);
  }

  void TearDown() override {
    controller_.reset();
    media_view_.reset();
    TestWithBrowserView::TearDown();
  }

  // ui::ComboboxModelObserver override
  void OnComboboxModelDestroying(ui::ComboboxModel* model) override {}
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    controller_->OnDeviceListChanged(actual_device_count_);
  }

  bool IsComboboxVisible() const {
    return controller_->device_selector_combobox_->GetVisible();
  }
  bool IsDeviceNameLabelVisible() const {
    return controller_->device_name_label_->GetVisible();
  }
  bool IsNoDeviceLabelVisible() const {
    return controller_->no_devices_found_label_->GetVisible();
  }

  const std::u16string& GetComboboxAccessibleName() const {
    return controller_->device_selector_combobox_->GetAccessibleName();
  }
  const std::u16string& GetDeviceNameLabel() const {
    return controller_->device_name_label_->GetText();
  }
  const std::u16string& GetNoDeviceLabel() const {
    return controller_->no_devices_found_label_->GetText();
  }

  void UpdateComboboxModel(size_t device_count) {
    actual_device_count_ = device_count;
    std::vector<ui::SimpleComboboxModel::Item> items;
    if (device_count == 0) {
      items.emplace_back(std::u16string());
    } else {
      for (size_t i = 1; i <= device_count; ++i) {
        items.emplace_back(GetDeviceName(i));
      }
    }
    combobox_model_->UpdateItemList(std::move(items));
  }

  size_t actual_device_count_ = 0;
  std::unique_ptr<MediaView> media_view_;
  std::unique_ptr<ui::SimpleComboboxModel> combobox_model_;
  base::MockCallback<MediaViewControllerBase::SourceChangeCallback>
      source_change_callback_;
  std::unique_ptr<MediaViewControllerBase> controller_;
};

TEST_F(MediaViewControllerBaseTest, OnDeviceListChanged_NoDevices) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_EQ(GetNoDeviceLabel(), kNoDeviceLabelText);
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), kNoDeviceComboboxText);
  EXPECT_FALSE(IsComboboxVisible());

  EXPECT_CALL(source_change_callback_, Run(_)).Times(0);
  UpdateComboboxModel(/*device_count=*/0);

  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_EQ(GetNoDeviceLabel(), kNoDeviceLabelText);
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), kNoDeviceComboboxText);
  EXPECT_FALSE(IsComboboxVisible());
}

TEST_F(MediaViewControllerBaseTest, OnDeviceListChanged_OneDevice) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_EQ(GetNoDeviceLabel(), kNoDeviceLabelText);
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), kNoDeviceComboboxText);
  EXPECT_FALSE(IsComboboxVisible());

  EXPECT_CALL(source_change_callback_, Run(Eq(0)));
  UpdateComboboxModel(/*device_count=*/1);

  EXPECT_FALSE(IsNoDeviceLabelVisible());
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), GetDeviceName(1));
  EXPECT_FALSE(IsComboboxVisible());
}

TEST_F(MediaViewControllerBaseTest, OnDeviceListChanged_MultipleDevices) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_EQ(GetNoDeviceLabel(), kNoDeviceLabelText);
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), kNoDeviceComboboxText);
  EXPECT_FALSE(IsComboboxVisible());

  EXPECT_CALL(source_change_callback_, Run(Eq(0)));
  UpdateComboboxModel(/*device_count=*/2);

  EXPECT_FALSE(IsNoDeviceLabelVisible());
  EXPECT_FALSE(IsDeviceNameLabelVisible());
  // No need to check for `GetDeviceNameLabel()` since it is not visible.
  EXPECT_TRUE(IsComboboxVisible());
  EXPECT_EQ(GetComboboxAccessibleName(), kComboboxAccessibleName);
}
