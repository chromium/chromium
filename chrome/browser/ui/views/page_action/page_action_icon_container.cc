// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"

#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

PageActionIconContainerView::PageActionIconContainerView(
    const PageActionIconParams& params)
    : controller_(std::make_unique<PageActionIconController>()) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          params.between_icon_spacing));
  // Right align to clip the leftmost items first when not enough space.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  controller_->Init(params, this);
}

PageActionIconContainerView::~PageActionIconContainerView() = default;

void PageActionIconContainerView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void PageActionIconContainerView::AddPageActionIcon(views::View* icon) {
  AddChildView(icon);
}

BEGIN_METADATA(PageActionIconContainerView, views::View)
END_METADATA
