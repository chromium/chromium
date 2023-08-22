// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

constexpr char kWidgetName[] = "EditorMenuPromoCardViewWidget";
constexpr char16_t kTitleTextPlaceholder[] =
    u"Editor menu title text placeholder";

constexpr int kContainerMinWidthDip = 368;

// Spacing between this view and the anchor view (context menu).
constexpr int kMarginDip = 8;

}  // namespace

EditorMenuPromoCardView::EditorMenuPromoCardView(
    const gfx::Rect& anchor_view_bounds) {
  InitLayout();
}

EditorMenuPromoCardView::~EditorMenuPromoCardView() = default;

// static
views::UniqueWidgetPtr EditorMenuPromoCardView::CreateWidget(
    const gfx::Rect& anchor_view_bounds) {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  EditorMenuPromoCardView* editor_menu_promo_card_view =
      widget->SetContentsView(
          std::make_unique<EditorMenuPromoCardView>(anchor_view_bounds));
  editor_menu_promo_card_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuPromoCardView::UpdateBounds(
    const gfx::Rect& anchor_view_bounds) {
  const int height = GetHeightForWidth(anchor_view_bounds.width());
  int y = anchor_view_bounds.y() - kMarginDip - height;

  // The Editor Menu view will be off screen if showing above the anchor.
  // Show below the anchor instead.
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds)
              .work_area()
              .y()) {
    y = anchor_view_bounds.bottom() + kMarginDip;
  }

  const gfx::Rect bounds = {{anchor_view_bounds.x(), y},
                            {kContainerMinWidthDip, height}};
  GetWidget()->SetBounds(bounds);
}

void EditorMenuPromoCardView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(kTitleTextPlaceholder);
}

void EditorMenuPromoCardView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  AddChildView(std::make_unique<views::Label>(
      kTitleTextPlaceholder, views::style::CONTEXT_DIALOG_TITLE));
}

BEGIN_METADATA(EditorMenuPromoCardView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
