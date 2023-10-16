// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_border.h"

namespace compose {

// Default size from Figma spec. The size of the view will follow the requested
// size of the WebUI, once these are connected.
constexpr gfx::Size kInputDialogSize(448, 216);

ComposeDialogView::~ComposeDialogView() = default;

ComposeDialogView::ComposeDialogView(View* anchor_view,
                                     views::BubbleBorder::Arrow anchor_position,
                                     const gfx::Rect bounds,
                                     content::WebContents* web_contents)
    : BubbleDialogDelegateView(anchor_view, anchor_position) {
  // For testing, a test Window widget is used. Otherwise, no |anchor_view| is
  // given and the parent Window must be manually set.
  if (!anchor_view) {
    set_parent_window(web_contents->GetNativeView());
    SetAnchorRect(bounds);
  }

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetPreferredSize(kInputDialogSize);

  // Empty contents, to be populated by WebUI.
}

BEGIN_METADATA(ComposeDialogView, views::View)
END_METADATA

}  // namespace compose
