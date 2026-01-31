// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_resize_area.h"

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/mojom/ax_node_data.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace views {

SidePanelResizeHandle::SidePanelResizeHandle(SidePanel* side_panel)
    : side_panel_(side_panel) {
  SetProperty(views::kElementIdentifierKey, kSidePanelResizeHandleElementId);
  SetVisible(false);
  gfx::Size preferred_size(4, 24);
  SetPreferredSize(preferred_size);
  SetCanProcessEventsWithinSubtree(false);

  const int resize_handle_left_margin = 2;
  SetProperty(views::kMarginsKey,
              gfx::Insets().set_left(resize_handle_left_margin));
}

void SidePanelResizeHandle::OnThemeChanged() {
  ImageView::OnThemeChanged();

  const SkColor resize_handle_color =
      GetColorProvider()->GetColor(kColorSidePanelHoverResizeAreaHandle);
  SetBackground(CreateRoundedRectBackground(resize_handle_color, 2));
}

BEGIN_METADATA(SidePanelResizeHandle)
END_METADATA

SidePanelResizeArea::SidePanelResizeArea(SidePanel* side_panel)
    : ResizeArea(side_panel), side_panel_(side_panel) {
  SetProperty(views::kElementIdentifierKey, kSidePanelResizeAreaElementId);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  auto* layout_manager = SetLayoutManager(std::make_unique<FlexLayout>());
  layout_manager->SetOrientation(LayoutOrientation::kVertical)
      .SetMainAxisAlignment(LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(LayoutAlignment::kStart);

  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_RESIZE));

  resize_handle_ =
      AddChildView(std::make_unique<SidePanelResizeHandle>(side_panel));

  FocusRing::Install(resize_handle_);
  FocusRing::Get(resize_handle_)
      ->SetHasFocusPredicate(base::BindRepeating([](const View* view) {
        return view->parent() && view->parent()->HasFocus();
      }));
}

void SidePanelResizeArea::OnMouseReleased(const ui::MouseEvent& event) {
  ResizeArea::OnMouseReleased(event);
  side_panel_->RecordMetricsIfResized();
}

bool SidePanelResizeArea::OnKeyPressed(const ui::KeyEvent& event) {
  const int resize_increment = 50;
  if (event.key_code() == ui::VKEY_LEFT) {
    side_panel_->OnResize(
        base::i18n::IsRTL() ? resize_increment : -resize_increment, true);
    side_panel_->SetKeyboardResized(true);
    return true;
  } else if (event.key_code() == ui::VKEY_RIGHT) {
    side_panel_->OnResize(
        base::i18n::IsRTL() ? -resize_increment : resize_increment, true);
    side_panel_->SetKeyboardResized(true);
    return true;
  }
  return false;
}

void SidePanelResizeArea::OnMouseMoved(const ui::MouseEvent& event) {
  UpdateHandleVisibility(true);
}

void SidePanelResizeArea::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHandleVisibility(true);
}

void SidePanelResizeArea::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHandleVisibility(HasFocus());
}

void SidePanelResizeArea::OnFocus() {
  // Update visibility before the base class implementation to ensure the handle
  // is visible when accessibility events (triggered by View::OnFocus) are
  // fired.
  UpdateHandleVisibility(true);
  if (auto* focus_ring = FocusRing::Get(resize_handle_)) {
    focus_ring->SchedulePaint();
  }
  View::OnFocus();
}

void SidePanelResizeArea::OnBlur() {
  // Update visibility before the base class implementation to ensure the handle
  // is not visible when accessibility events (triggered by View::OnBlur) are
  // fired.
  UpdateHandleVisibility(IsMouseHovered());
  if (auto* focus_ring = FocusRing::Get(resize_handle_)) {
    focus_ring->SchedulePaint();
  }
  View::OnBlur();
  // The user has finished interacting with the resize area (potentially via
  // keyboard), so we record the final resize metrics here.
  side_panel_->RecordMetricsIfResized();
  // Set keyboard resized to false to catch any cases when the previous
  // attempt to resize via keyboard didn't change the bounds to be captured.
  // This could happen if attempting to change the side panel size to be
  // smaller than the minimum size or large enough where the page content
  // would be below minimum size.
  side_panel_->SetKeyboardResized(false);
}

void SidePanelResizeArea::Layout(PassKey) {
  LayoutSuperclass<ResizeArea>(this);
  // The side panel resize area should draw on top of its parent's border.
  gfx::Rect local_bounds = parent()->GetLocalBounds();
  gfx::Rect contents_bounds = parent()->GetContentsBounds();

  gfx::Rect resize_bounds;
  if ((side_panel_->IsRightAligned() && !base::i18n::IsRTL()) ||
      (!side_panel_->IsRightAligned() && base::i18n::IsRTL())) {
    resize_bounds = gfx::Rect(local_bounds.x(), local_bounds.y(),
                              contents_bounds.x(), local_bounds.height());
  } else {
    resize_bounds = gfx::Rect(contents_bounds.right(), local_bounds.y(),
                              local_bounds.right() - contents_bounds.right(),
                              local_bounds.height());
  }

  SetBoundsRect(resize_bounds);
}

void SidePanelResizeArea::UpdateHandleVisibility(bool visible) {
  resize_handle_->SetVisible(visible);
}

BEGIN_METADATA(SidePanelResizeArea)
END_METADATA

}  // namespace views
