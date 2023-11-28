// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_border.h"

ComposeDialogView::~ComposeDialogView() = default;

ComposeDialogView::ComposeDialogView(
    View* anchor_view,
    std::unique_ptr<BubbleContentsWrapperT<ComposeUI>> bubble_wrapper,
    const gfx::Rect& anchor_bounds,
    views::BubbleBorder::Arrow anchor_position)
    : WebUIBubbleDialogView(anchor_view,
                            bubble_wrapper.get(),
                            anchor_bounds,
                            anchor_position),
      bubble_wrapper_(std::move(bubble_wrapper)) {
  set_has_parent(false);
}

base::WeakPtr<ComposeDialogView> ComposeDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(ComposeDialogView, views::View)
END_METADATA
