// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <memory>

#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

namespace {

constexpr char kWidgetName[] = "MahiMenuViewWidget";

}  // namespace

MahiMenuView::MahiMenuView() {
  SetLayoutManager(std::make_unique<views::FlexLayout>());

  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorPrimaryBackground,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius)));

  // TODO(b/318733118): Finish building the menu UI.
  AddChildView(std::make_unique<views::Label>(u"Mahi Menu"));
}

MahiMenuView::~MahiMenuView() = default;

// static
views::UniqueWidgetPtr MahiMenuView::CreateWidget(
    const gfx::Rect& anchor_view_bounds) {
  views::Widget::InitParams params;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  MahiMenuView* editor_menu_view =
      widget->SetContentsView(std::make_unique<MahiMenuView>());
  editor_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void MahiMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  const int menu_width = anchor_view_bounds.width();

  // TODO(b/318733414): Move `editor_menu::GetEditorMenuBounds` to a common
  // place for use
  GetWidget()->SetBounds(editor_menu::GetEditorMenuBounds(
      anchor_view_bounds,
      gfx::Size(menu_width, GetHeightForWidth(menu_width))));
}

BEGIN_METADATA(MahiMenuView, views::View)
END_METADATA

}  // namespace chromeos::mahi
