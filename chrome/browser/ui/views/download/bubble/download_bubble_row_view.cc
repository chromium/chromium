// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/display/screen.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<views::View> CreateLabelWrapper() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto label_wrapper = std::make_unique<views::View>();
  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets(0, icon_label_spacing));
  label_wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));
  return label_wrapper;
}

std::u16string GetSecondaryLabelCompletedDownload(
    const DownloadUIModel* model) {
  std::u16string total_text = ui::FormatBytes(model->GetTotalBytes());
  std::u16string delta_str = ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      base::Time::Now() - model->GetEndTime());

  return base::StrCat(
      {total_text,
       l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DOWNLOAD_SEPERATOR),
       delta_str});
}

std::u16string GetSecondaryLabel(DownloadUIModel* model) {
  switch (model->GetState()) {
    case download::DownloadItem::COMPLETE:
      return GetSecondaryLabelCompletedDownload(model);
    case download::DownloadItem::IN_PROGRESS:
    case download::DownloadItem::INTERRUPTED:
    case download::DownloadItem::CANCELLED:
      return std::u16string();
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      return std::u16string();
  }
}
}  // namespace

DownloadBubbleRowView::~DownloadBubbleRowView() = default;

void DownloadBubbleRowView::AddedToWidget() {
  const display::Screen* const screen = display::Screen::GetScreen();
  current_scale_ = screen->GetDisplayNearestView(GetWidget()->GetNativeView())
                       .device_scale_factor();
  LoadIcon();
}

void DownloadBubbleRowView::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  current_scale_ = new_device_scale_factor;
  LoadIcon();
}

void DownloadBubbleRowView::SetIcon(gfx::Image icon) {
  icon_->SetImage(ui::ImageModel::FromImage(icon));
}

void DownloadBubbleRowView::LoadIcon() {
  // The correct scale_factor is set only in the AddedToWidget()
  if (!GetWidget())
    return;

  base::FilePath file_path = model_->GetTargetFilePath();

  IconManager* const im = g_browser_process->icon_manager();
  const gfx::Image* const file_icon_image =
      im->LookupIconFromFilepath(file_path, IconLoader::SMALL, current_scale_);

  if (file_icon_image) {
    SetIcon(*file_icon_image);
  } else {
    im->LoadIcon(file_path, IconLoader::SMALL, current_scale_,
                 base::BindOnce(&DownloadBubbleRowView::SetIcon,
                                weak_factory_.GetWeakPtr()),
                 &cancelable_task_tracker_);
  }
}

DownloadBubbleRowView::DownloadBubbleRowView(
    DownloadUIModel::DownloadUIModelPtr model)
    : model_(std::move(model)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetProperty(views::kMarginsKey,
              gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));

  auto* label_wrapper = AddChildView(CreateLabelWrapper());
  auto* primary_label =
      label_wrapper->AddChildView(std::make_unique<views::Label>(
          model_->GetFileNameToReportUser().LossyDisplayName(),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  primary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  primary_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));

  auto* secondary_label =
      label_wrapper->AddChildView(std::make_unique<views::Label>(
          GetSecondaryLabel(model_.get()), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
  secondary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));
}

BEGIN_METADATA(DownloadBubbleRowView, views::View)
END_METADATA
