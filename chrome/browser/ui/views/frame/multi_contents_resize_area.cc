// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/mojom/ax_node_data.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
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
  // Cannot use `SOLID_COLOR_LAYER`, since resize handle has rounded corners,
  // and applying layer based rounded corners will clip the focus ring.
  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);

  SetPreferredSize(gfx::Size(kHandleWidth, kHandleHeight));
  SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SPLIT_TABS_RESIZE));
  SetProperty(views::kElementIdentifierKey,
              kMultiContentsResizeHandleElementId);

  SetBackground(views::CreateRoundedRectBackground(
      kColorSidePanelHoverResizeAreaHandle, kHandleCornerRadius));
}

void MultiContentsResizeHandle::UpdateVisibility() {
  layer()->SetVisible(HasFocus() || parent()->IsMouseHovered());
}

void MultiContentsResizeHandle::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void MultiContentsResizeHandle::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void MultiContentsResizeHandle::OnDidChangeFocus(views::View* before,
                                                 views::View* now) {
  UpdateVisibility();
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

void MultiContentsResizeArea::OnGestureEvent(ui::GestureEvent* event) {
  // If the gesture event was a double tap and was not part of a resizing event,
  // swap the contents views.
  if (!is_resizing() && event->type() == ui::EventType::kGestureTap &&
      event->details().tap_count() == 2) {
    multi_contents_view_->OnSwap();
  }
  ResizeArea::OnGestureEvent(event);
}

void MultiContentsResizeArea::OnMouseReleased(const ui::MouseEvent& event) {
  // If the mouse event was a left double click and was not part of a resizing
  // event, swap the contents views.
  if (!is_resizing() && event.IsOnlyLeftMouseButton() &&
      event.GetClickCount() == 2) {
    multi_contents_view_->OnSwap();
  }
  ResizeArea::OnMouseReleased(event);
}

bool MultiContentsResizeArea::OnKeyPressed(const ui::KeyEvent& event) {
  const int resize_increment = 50;
  if (event.key_code() == ui::VKEY_LEFT) {
    multi_contents_view_->OnResize(
        base::i18n::IsRTL() ? resize_increment : -resize_increment, true);
    return true;
  } else if (event.key_code() == ui::VKEY_RIGHT) {
    multi_contents_view_->OnResize(
        base::i18n::IsRTL() ? -resize_increment : resize_increment, true);
    return true;
  }
  return false;
}

void MultiContentsResizeArea::OnMouseMoved(const ui::MouseEvent& event) {
  resize_handle_->UpdateVisibility();
}

void MultiContentsResizeArea::OnMouseExited(const ui::MouseEvent& event) {
  resize_handle_->UpdateVisibility();
}

BEGIN_METADATA(MultiContentsResizeArea)
END_METADATA
