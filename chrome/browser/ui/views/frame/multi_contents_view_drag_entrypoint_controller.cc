// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drag_entrypoint_controller.h"

#include "content/public/common/drop_data.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewDropTargetElementId);

MultiContentsViewDragEntrypointController::
    MultiContentsViewDragEntrypointController(views::View& multi_contents_view)
    : multi_contents_view_(multi_contents_view),
      drop_target_view_(
          *multi_contents_view.AddChildView(std::make_unique<views::View>())) {
  drop_target_view_->SetProperty(views::kElementIdentifierKey,
                                 kMultiContentsViewDropTargetElementId);
  drop_target_view_->SetVisible(false);
}

void MultiContentsViewDragEntrypointController::OnWebContentsDragUpdate(
    const content::DropData& data,
    const gfx::PointF& point) {
  CHECK_LE(point.x(), multi_contents_view_->width());

  // TODO(crbug.com/394369035): Settle on an appropriate value for this.
  constexpr int kDropEntryPointWidth = 100;

  const bool should_show_drop_zone =
      data.url.is_valid() &&
      point.x() >= multi_contents_view_->width() - kDropEntryPointWidth;
  // TODO(crbug.com/394369035): Add a timer to delay showing the drop zone.
  drop_target_view_->SetVisible(should_show_drop_zone);
}
