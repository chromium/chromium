// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/display/screen.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/root_view.h"
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
}  // namespace

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
  // PreferredSizeChanged();
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

DownloadBubbleRowView::~DownloadBubbleRowView() {
  model_->RemoveObserver(this);
}

DownloadBubbleRowView::DownloadBubbleRowView(
    DownloadUIModel::DownloadUIModelPtr model,
    DownloadBubbleRowListView* row_list_view,
    DownloadBubbleUIController* bubble_controller)
    : model_(std::move(model)),
      context_menu_(
          std::make_unique<DownloadShelfContextMenuView>(model_->GetWeakPtr())),
      row_list_view_(row_list_view),
      bubble_controller_(bubble_controller) {
  model_->AddObserver(this);
  set_context_menu_controller(this);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  auto* main_row = AddChildView(std::make_unique<views::View>());
  main_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  main_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  icon_ = main_row->AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));

  auto* label_wrapper = main_row->AddChildView(CreateLabelWrapper());
  primary_label_ = label_wrapper->AddChildView(std::make_unique<views::Label>(
      model_->GetFileNameToReportUser().LossyDisplayName(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  primary_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));

  secondary_label_ = label_wrapper->AddChildView(std::make_unique<views::Label>(
      model_->GetStatusText(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  secondary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));

  if (model_->GetState() ==
      download::DownloadItem::DownloadState::IN_PROGRESS) {
    cancel_button_ =
        main_row->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&DownloadBubbleRowView::OnCancelButtonPressed,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CANCEL)));
    progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
    progress_bar_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kScaleToMaximum,
                                 /*adjust_height_for_width=*/true)
            .WithWeight(1));
    progress_bar_->SetProperty(
        views::kMarginsKey,
        gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  }
}

void DownloadBubbleRowView::OnCancelButtonPressed() {
  bubble_controller_->RemoveContentIdFromPartialView(model_->GetContentId());
  model_->Cancel(/*user_cancel=*/true);
}

void DownloadBubbleRowView::OnDownloadUpdated() {
  primary_label_->SetText(model_->GetFileNameToReportUser().LossyDisplayName());
  secondary_label_->SetText(model_->GetStatusText());
  LoadIcon();
  if (model_->GetState() ==
      download::DownloadItem::DownloadState::IN_PROGRESS) {
    progress_bar_->SetValue(static_cast<double>(model_->PercentComplete()) /
                            100);
  } else if (cancel_button_.get()) {
    cancel_button_->parent()->RemoveChildViewT(cancel_button_);
    cancel_button_ = nullptr;
    RemoveChildViewT(progress_bar_);
    progress_bar_ = nullptr;
  }
  // PreferredSizeChanged();
}

void DownloadBubbleRowView::OnDownloadOpened() {
  bubble_controller_->RemoveContentIdFromPartialView(model_->GetContentId());
}

void DownloadBubbleRowView::OnDownloadDestroyed() {
  // This will return ownership and destroy this object at the end of the
  // method.
  auto row_view_ptr = row_list_view_->RemoveChildViewT(this);
}

void DownloadBubbleRowView::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Similar hack as in MenuButtonController and DownloadItemView.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to null.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())
      ->SetMouseAndGestureHandler(nullptr);

  context_menu_->Run(GetWidget()->GetTopLevelWidget(),
                     gfx::Rect(point, gfx::Size()), source_type,
                     base::RepeatingClosure());
}

BEGIN_METADATA(DownloadBubbleRowView, views::View)
END_METADATA
