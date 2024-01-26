// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

namespace {

constexpr char kWidgetName[] = "MahiMenuViewWidget";

constexpr int kButtonHeight = 20;

}  // namespace

MahiMenuView::MahiMenuView() {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorPrimaryBackground,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius)));

  // TODO(b/318733118): Finish building the menu UI.
  // TODO(b/319264190): Replace the strings here with real strings.
  AddChildView(std::make_unique<views::Label>(u"Mahi Menu"));

  auto* button = AddChildView(std::make_unique<views::LabelButton>(
      /*callback=*/base::BindRepeating(&MahiMenuView::OnSummaryButtonPressed,
                                       weak_ptr_factory_.GetWeakPtr()),
      /*text=*/u"Show"));
  button->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBase, 4));
  button->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kInfoOutlineIcon,
                                     ui::kColorMenuIcon, kButtonHeight));
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
  MahiMenuView* mahi_menu_view =
      widget->SetContentsView(std::make_unique<MahiMenuView>());
  mahi_menu_view->UpdateBounds(anchor_view_bounds);

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

void MahiMenuView::OnSummaryButtonPressed() {
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  chromeos::MahiManager::Get()->OpenMahiPanel(display.id());
}

BEGIN_METADATA(MahiMenuView, views::View)
END_METADATA

}  // namespace chromeos::mahi
