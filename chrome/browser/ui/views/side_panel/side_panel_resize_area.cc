// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_resize_area.h"

#include "base/i18n/rtl.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/mojom/ax_node_data.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"

namespace views {

SidePanelResizeHandle::SidePanelResizeHandle(SidePanel* side_panel)
    : side_panel_(side_panel) {
  const gfx::Size preferred_resize_handle_size = gfx::Size(16, 24);
  SetPreferredSize(preferred_resize_handle_size);
  SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  FocusRing::Install(this);
}

void SidePanelResizeHandle::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kSlider;
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_RESIZE));
}

void SidePanelResizeHandle::OnThemeChanged() {
  ImageView::OnThemeChanged();
  const int icon_size = 16;
  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      kDragHandleIcon, icon_size,
      GetColorProvider()->GetColor(kColorToolbarContentAreaSeparator));
  SetImage(icon);
}

void SidePanelResizeHandle::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void SidePanelResizeHandle::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void SidePanelResizeHandle::OnWillChangeFocus(views::View* before,
                                              views::View* now) {}

void SidePanelResizeHandle::OnDidChangeFocus(views::View* before,
                                             views::View* now) {
  if (before == this && now != this) {
    side_panel_->RecordMetricsIfResized();
  }
}

BEGIN_METADATA(SidePanelResizeHandle, ImageView)
END_METADATA

SidePanelResizeArea::SidePanelResizeArea(SidePanel* side_panel)
    : ResizeArea(side_panel), side_panel_(side_panel) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto* layout_manager = SetLayoutManager(std::make_unique<FlexLayout>());
  layout_manager->SetOrientation(LayoutOrientation::kVertical)
      .SetMainAxisAlignment(LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(LayoutAlignment::kStart);

  resize_handle_ =
      AddChildView(std::make_unique<SidePanelResizeHandle>(side_panel));
}

void SidePanelResizeArea::OnMouseReleased(const ui::MouseEvent& event) {
  ResizeArea::OnMouseReleased(event);
  side_panel_->RecordMetricsIfResized();
}

bool SidePanelResizeArea::OnKeyPressed(const ui::KeyEvent& event) {
  const int resize_increment = 50;
  if (event.key_code() == ui::VKEY_LEFT) {
    side_panel_->OnResize(-resize_increment, true);
    return true;
  } else if (event.key_code() == ui::VKEY_RIGHT) {
    side_panel_->OnResize(resize_increment, true);
    return true;
  }
  return false;
}

void SidePanelResizeArea::Layout() {
  ResizeArea::Layout();
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
                              local_bounds.right(), local_bounds.height());
  }

  SetBoundsRect(resize_bounds);
}

BEGIN_METADATA(SidePanelResizeArea, ResizeArea)
END_METADATA

}  // namespace views
