// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"

#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

PageActionIconContainerView::PageActionIconContainerView(
    const PageActionIconParams& params)
    : controller_(std::make_unique<PageActionIconController>()) {
  SetBetweenChildSpacing(params.between_icon_spacing);
  // Right align to clip the leftmost items first when not enough space.
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);

  controller_->Init(params, this);
}

PageActionIconContainerView::~PageActionIconContainerView() = default;

void PageActionIconContainerView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void PageActionIconContainerView::AddPageActionIcon(
    std::unique_ptr<views::View> icon) {
  AddChildView(std::move(icon));
}

BEGIN_METADATA(PageActionIconContainerView)
END_METADATA
