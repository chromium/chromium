// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/combobox_test_api.h"
#include "ui/views/widget/unique_widget_ptr.h"

using testing::_;
using testing::Eq;
using UserAction = media_preview_metrics::MediaPreviewDeviceSelectionUserAction;

namespace {

constexpr char16_t kNoDeviceComboboxText[] = u"no_device_combobox_text";
constexpr char16_t kNoDeviceLabelText[] = u"no_device_label_text";
constexpr char16_t kComboboxAccessibleName[] = u"combobox_accessible_name";

std::u16string GetDeviceName(size_t index) {
  return base::UTF8ToUTF16(base::StringPrintf("device_%zu", index));
}

media_preview_metrics::Context GetMetricsContext() {
  return {media_preview_metrics::UiLocation::kPermissionPrompt,
          media_preview_metrics::PreviewType::kCamera};
}

#if !BUILDFLAG(IS_MAC)
std::optional<std::u16string> GetAnnouncementFromRootView(
    views::View* root_view) {
  if (!root_view || root_view->children().size() < 2u) {
    return std::nullopt;
  }
  ui::AXNodeData node_data;
  views::View* const hidden_polite_view = root_view->children()[1];
  hidden_polite_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
}
#endif

}  // namespace

class MediaViewControllerBaseTestParameterized
    : public ChromeViewsTestBase,
      public ui::ComboboxModelObserver,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    allow_device_selection_ = GetParam();
    media_view_ = std::make_unique<MediaView>();
    combobox_model_ = std::make_unique<ui::SimpleComboboxModel>(
        std::vector<ui::SimpleComboboxModel::Item>());
    controller_ = std::make_unique<MediaViewControllerBase>(
        *media_view_, /*needs_borders=*/true, combobox_model_.get(),
        source_change_callback_.Get(),
        /*combobox_accessible_name=*/kComboboxAccessibleName,
        /*no_devices_found_combobox_text=*/kNoDeviceComboboxText,
        /*no_devices_found_label_text=*/kNoDeviceLabelText,
        /*allow_device_selection=*/allow_device_selection_,
        GetMetricsContext());
    combobox_model_->AddObserver(this);
    combobox_test_api_.emplace(&controller_->GetComboboxForTesting().get());
  }

  void TearDown() override {
    combobox_test_api_.reset();
    controller_.reset();
    media_view_.reset();
    if (widget_) {
      widget_->Close();
    }
    ChromeViewsTestBase::TearDown();
  }

  void InitializeWidget() {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params =
        CreateParams(views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_->Init(std::move(init_params));
    widget_->Show();
    widget_->SetContentsView(std::move(media_view_));
  }

  // ui::ComboboxModelObserver override
  void OnComboboxModelDestroying(ui::ComboboxModel* model) override {}
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    controller_->OnDeviceListChanged(model->GetItemCount());
  }

  bool IsComboboxVisible() const {
    return controller_->GetComboboxForTesting()->GetVisible();
  }
  bool IsDeviceNameLabelVisible() const {
    return controller_->GetDeviceNameLabelViewForTesting()->GetVisible();
  }
  bool IsNoDeviceLabelVisible() const {
    return controller_->GetNoDeviceLabelViewForTesting()->GetVisible();
  }

  std::u16string GetComboboxAccessibleName() const {
    return controller_->GetComboboxForTesting()
        ->GetViewAccessibility()
        .GetCachedName();
  }

  const std::u16string& GetDeviceNameLabel() const {
    return controller_->GetDeviceNameLabelViewForTesting()->GetText();
  }

  const std::u16string& GetNoDeviceLabel() const {
    return controller_->GetNoDeviceLabelViewForTesting()->GetText();
  }

  void UpdateComboboxModel(size_t device_count) {
    std::vector<ui::SimpleComboboxModel::Item> items;
    for (size_t i = 1; i <= device_count; ++i) {
      items.emplace_back(GetDeviceName(i));
    }
    combobox_model_->UpdateItemList(std::move(items));
  }

  const raw_ref<views::Label> GetDeviceNameLabelView() {
    return controller_->GetDeviceNameLabelViewForTesting();
  }

  void TriggerComboboxMenuWillShow() { controller_->OnComboboxMenuWillShow(); }

  void ExpectHistogramUserAction(UserAction action) {
    const std::string histogram_name =
        "MediaPreviews.UI.DeviceSelection.Permissions.Camera.Action";
    if (allow_device_selection_) {
      histogram_tester_.ExpectUniqueSample(histogram_name, action,
                                           /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectTotalCount(histogram_name, 0);
    }
  }

  base::HistogramTester histogram_tester_;
  bool allow_device_selection_ = false;
  views::UniqueWidgetPtr widget_;
  std::unique_ptr<MediaView> media_view_;
  std::unique_ptr<ui::SimpleComboboxModel> combobox_model_;
  base::MockCallback<MediaViewControllerBase::SourceChangeCallback>
      source_change_callback_;
  std::unique_ptr<MediaViewControllerBase> controller_;
  std::optional<views::test::ComboboxTestApi> combobox_test_api_;
};

INSTANTIATE_TEST_SUITE_P(MediaViewControllerBaseTest,
                         MediaViewControllerBaseTestParameterized,
                         testing::Bool());

TEST_P(MediaViewControllerBaseTestParameterized,
       OnDeviceListChanged_NoDevices) {
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

TEST_P(MediaViewControllerBaseTestParameterized,
       OnDeviceListChanged_OneDevice) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_EQ(GetNoDeviceLabel(), kNoDeviceLabelText);
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), kNoDeviceComboboxText);
  EXPECT_FALSE(IsComboboxVisible());

  EXPECT_CALL(source_change_callback_, Run(Eq(0)));
  UpdateComboboxModel(/*device_count=*/1);

  EXPECT_FALSE(IsNoDeviceLabelVisible());
  if (allow_device_selection_) {
    EXPECT_FALSE(IsDeviceNameLabelVisible());
    EXPECT_TRUE(IsComboboxVisible());
  } else {
    EXPECT_FALSE(IsComboboxVisible());
    EXPECT_TRUE(IsDeviceNameLabelVisible());
    EXPECT_EQ(GetDeviceNameLabel(), GetDeviceName(1));
  }
}

TEST_P(MediaViewControllerBaseTestParameterized,
       OnDeviceListChanged_MultipleDevices) {
  EXPECT_TRUE(IsNoDeviceLabelVisible());
  EXPECT_EQ(GetNoDeviceLabel(), kNoDeviceLabelText);
  EXPECT_TRUE(IsDeviceNameLabelVisible());
  EXPECT_EQ(GetDeviceNameLabel(), kNoDeviceComboboxText);
  EXPECT_FALSE(IsComboboxVisible());

  EXPECT_CALL(source_change_callback_, Run(Eq(0)));
  UpdateComboboxModel(/*device_count=*/2);

  EXPECT_FALSE(IsNoDeviceLabelVisible());
  if (allow_device_selection_) {
    EXPECT_FALSE(IsDeviceNameLabelVisible());
    // No need to check for `GetDeviceNameLabel()` since it is not visible.
    EXPECT_TRUE(IsComboboxVisible());
    EXPECT_EQ(GetComboboxAccessibleName(), kComboboxAccessibleName);
  } else {
    EXPECT_FALSE(IsComboboxVisible());
    EXPECT_TRUE(IsDeviceNameLabelVisible());
    EXPECT_EQ(GetDeviceNameLabel(), GetDeviceName(1));
  }
}

#if !BUILDFLAG(IS_MAC)
TEST_P(MediaViewControllerBaseTestParameterized,
       OnDeviceListChanged_Announcement) {
  InitializeWidget();
  auto* const widget = GetDeviceNameLabelView()->GetWidget();
  ASSERT_TRUE(widget);
  auto* const root_view = widget->GetRootView();
  ASSERT_TRUE(root_view);
  ui::AXNodeData node_data;

  EXPECT_TRUE(IsNoDeviceLabelVisible());

  // No announcement because `has_device_list_changed_before_` is false.
  UpdateComboboxModel(/*device_count=*/2);
  EXPECT_EQ(std::nullopt, GetAnnouncementFromRootView(root_view));

  // No announcement because selected device name is equal to
  // `previous_device_name`.
  UpdateComboboxModel(/*device_count=*/1);
  EXPECT_EQ(std::nullopt, GetAnnouncementFromRootView(root_view));

  UpdateComboboxModel(/*device_count=*/0);
  if (allow_device_selection_) {
    // Announcement expected with `kNoDeviceLabelText` value.
    EXPECT_EQ(kNoDeviceLabelText, GetAnnouncementFromRootView(root_view));
  } else {
    EXPECT_EQ(std::nullopt, GetAnnouncementFromRootView(root_view));
  }

  const std::u16string device_1_announcement = l10n_util::GetStringFUTF16(
      IDS_MEDIA_PREVIEW_ANNOUNCE_SELECTED_DEVICE_CHANGE, GetDeviceName(1));

  UpdateComboboxModel(/*device_count=*/2);
  if (allow_device_selection_) {
    // Announcement expected for `device_1`.
    EXPECT_EQ(device_1_announcement, GetAnnouncementFromRootView(root_view));
  } else {
    EXPECT_EQ(std::nullopt, GetAnnouncementFromRootView(root_view));
  }

  combobox_test_api_->PerformActionAt(1);  // Selected device is `device_2`.
  UpdateComboboxModel(/*device_count=*/1);
  if (allow_device_selection_) {
    // Announcement expected for `device_1`.
    EXPECT_EQ(device_1_announcement, GetAnnouncementFromRootView(root_view));
  } else {
    EXPECT_EQ(std::nullopt, GetAnnouncementFromRootView(root_view));
  }
}
#endif

TEST_P(MediaViewControllerBaseTestParameterized, ActionHistogram_NoAction) {
  controller_.reset();
  ExpectHistogramUserAction(UserAction::kNoAction);
}

TEST_P(MediaViewControllerBaseTestParameterized, ActionHistogram_Opened) {
  UpdateComboboxModel(/*device_count=*/2);
  TriggerComboboxMenuWillShow();
  combobox_test_api_->PerformActionAt(0);
  controller_.reset();
  ExpectHistogramUserAction(UserAction::kOpened);
}

TEST_P(MediaViewControllerBaseTestParameterized, ActionHistogram_Selection) {
  UpdateComboboxModel(/*device_count=*/2);
  TriggerComboboxMenuWillShow();
  combobox_test_api_->PerformActionAt(1);
  controller_.reset();
  ExpectHistogramUserAction(UserAction::kSelection);
}
