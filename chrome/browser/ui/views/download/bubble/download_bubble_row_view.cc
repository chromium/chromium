// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "base/files/file_path.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

DownloadBubbleRowView::~DownloadBubbleRowView() {
  model_.reset();
}

DownloadBubbleRowView::DownloadBubbleRowView(
    DownloadUIModel::DownloadUIModelPtr model)
    : model_(std::move(model)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  auto title = std::make_unique<views::Label>(
      model_->GetFileNameToReportUser().LossyDisplayName(),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  AddChildView(std::move(title));
}

BEGIN_METADATA(DownloadBubbleRowView, views::View)
END_METADATA
