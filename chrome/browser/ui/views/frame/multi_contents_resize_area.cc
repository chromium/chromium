// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/mojom/ax_node_data.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"

namespace {
const int kHandleCornerRadius = 2;
const int kHandleHeight = 24;
const int kHandlePadding = 6;
const int kHandleWidth = 4;
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsResizeHandle,
                                      kMultiContentsResizeHandleElementId);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsResizeArea,
                                      kMultiContentsResizeAreaElementId);

MultiContentsResizeHandle::MultiContentsResizeHandle() {
  SetPreferredSize(gfx::Size(kHandleWidth, kHandleHeight));
  SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SPLIT_TABS_RESIZE));
  SetProperty(views::kElementIdentifierKey,
              kMultiContentsResizeHandleElementId);
}

void MultiContentsResizeHandle::UpdateVisibility(bool visible) {
  if (visible) {
    const SkColor resize_handle_color =
        GetColorProvider()->GetColor(kColorSidePanelHoverResizeAreaHandle);
    SetBackground(views::CreateRoundedRectBackground(resize_handle_color,
                                                     kHandleCornerRadius));
  } else {
    SetBackground(nullptr);
  }
}

void MultiContentsResizeHandle::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void MultiContentsResizeHandle::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void MultiContentsResizeHandle::OnWillChangeFocus(views::View* before,
                                                  views::View* now) {
  UpdateVisibility(now == this);
}

BEGIN_METADATA(MultiContentsResizeHandle)
END_METADATA

MultiContentsResizeArea::MultiContentsResizeArea(
    MultiContentsView* multi_contents_view)
    : ResizeArea(multi_contents_view),
      multi_contents_view_(multi_contents_view) {
  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  resize_handle_ = AddChildView(std::make_unique<MultiContentsResizeHandle>());

  SetProperty(views::kElementIdentifierKey, kMultiContentsResizeAreaElementId);
  SetPreferredSize(gfx::Size(kHandleWidth + kHandlePadding, kHandleHeight));
}

bool MultiContentsResizeArea::OnKeyPressed(const ui::KeyEvent& event) {
  const int resize_increment = 50;
  if (event.key_code() == ui::VKEY_LEFT) {
    multi_contents_view_->OnResize(-resize_increment, true);
    return true;
  } else if (event.key_code() == ui::VKEY_RIGHT) {
    multi_contents_view_->OnResize(resize_increment, true);
    return true;
  }
  return false;
}

void MultiContentsResizeArea::OnMouseMoved(const ui::MouseEvent& event) {
  resize_handle_->UpdateVisibility(true);
}

void MultiContentsResizeArea::OnMouseExited(const ui::MouseEvent& event) {
  resize_handle_->UpdateVisibility(false);
}

BEGIN_METADATA(MultiContentsResizeArea)
END_METADATA
