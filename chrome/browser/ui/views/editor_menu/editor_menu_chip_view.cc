// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"

#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/style/typography.h"

namespace chromeos::editor_menu {

namespace {

constexpr int kIconSizeDip = 16;
constexpr gfx::Insets kChipInsets = gfx::Insets::VH(6, 8);

}  // namespace

EditorMenuChipView::EditorMenuChipView(views::Button::PressedCallback callback,
                                       const PresetTextQuery& preset_text_query)
    : views::LabelButton(std::move(callback), preset_text_query.name),
      icon_(&GetIconForPresetQueryCategory(preset_text_query.category)) {
  CHECK(icon_);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColorId(ui::kColorIcon);
  SetHasInkDropActionOnClick(true);
  SetFocusRingCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh));

  SetTooltipText(preset_text_query.name);
  SetImageLabelSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_VECTOR_ICON_PADDING));
}

EditorMenuChipView::~EditorMenuChipView() = default;

void EditorMenuChipView::AddedToWidget() {
  // Only initialize the button after the button is added to a widget.
  InitLayout();
}

void EditorMenuChipView::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  UpdateBackgroundColor();
}

void EditorMenuChipView::InitLayout() {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
  label()->SetEnabledColorId(ui::kColorSysOnSurface);
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon_, ui::kColorSysOnSurface,
                                               kIconSizeDip));

  SetBorder(views::CreateEmptyBorder(kChipInsets));
  UpdateBackgroundColor();

  PreferredSizeChanged();
}

void EditorMenuChipView::UpdateBackgroundColor() {
  SetBackground(CreateBackgroundFromPainter(
      views::Painter::CreateRoundRectWith1PxBorderPainter(
          SK_ColorTRANSPARENT,
          GetColorProvider()->GetColor(ui::kColorSysTonalOutline),
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::Emphasis::kHigh))));
}

BEGIN_METADATA(EditorMenuChipView, views::LabelButton)
END_METADATA

}  // namespace chromeos::editor_menu
