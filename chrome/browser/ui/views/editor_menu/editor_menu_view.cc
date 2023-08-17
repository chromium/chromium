// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"

#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

constexpr char kWidgetName[] = "EditorMenuViewWidget";
constexpr char16_t kContainerTitle[] = u"Editor Menu";

constexpr int kContainerMinWidthDip = 368;
constexpr int kRadiusDip = 4;

constexpr gfx::Insets kTitleContainerInsets = gfx::Insets::TLBR(10, 16, 10, 10);

// Spacing between this view and the anchor view (context menu).
constexpr int kMarginDip = 8;

}  // namespace

EditorMenuView::EditorMenuView(const gfx::Rect& anchor_view_bounds) {
  InitLayout();
}

EditorMenuView::~EditorMenuView() = default;

// static
views::UniqueWidgetPtr EditorMenuView::CreateWidget(
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
  EditorMenuView* editor_menu_view = widget->SetContentsView(
      std::make_unique<EditorMenuView>(anchor_view_bounds));
  editor_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  int height = GetHeightForWidth(anchor_view_bounds.width());
  int y = anchor_view_bounds.y() - kMarginDip - height;

  // The Editor Menu view will be off screen if showing above the anchor.
  // Show below the anchor instead.
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds)
              .work_area()
              .y()) {
    y = anchor_view_bounds.bottom() + kMarginDip;
  }

  gfx::Rect bounds = {{anchor_view_bounds.x(), y},
                      {kContainerMinWidthDip, height}};
  GetWidget()->SetBounds(bounds);
}

void EditorMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(kContainerTitle);
}

void EditorMenuView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysAppBase, kRadiusDip));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  AddTitleContainer();
}

void EditorMenuView::AddTitleContainer() {
  title_container_ = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* layout =
      title_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* title = title_container_->AddChildView(
      std::make_unique<views::Label>(kContainerTitle));

  // TODO(b/295078199): Add Settings icon.

  title_container_->SetProperty(views::kMarginsKey, kTitleContainerInsets);
  title_container_->SetPreferredSize(
      gfx::Size(kContainerMinWidthDip - kTitleContainerInsets.width(),
                title->GetPreferredSize().height()));
}

BEGIN_METADATA(EditorMenuView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
