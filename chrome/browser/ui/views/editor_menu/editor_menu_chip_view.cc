// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"

#include "chrome/browser/ui/views/editor_menu/editor_menu_strings.h"
#include "chromeos/components/editor_menu/public/cpp/icon.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace chromeos::editor_menu {

namespace {

constexpr int kIconSizeDip = 16;
constexpr gfx::Insets kChipInsets = gfx::Insets::VH(6, 8);

}  // namespace

EditorMenuChipView::EditorMenuChipView(views::Button::PressedCallback callback,
                                       const PresetTextQuery& preset_text_query)
    : views::MdTextButton(std::move(callback), preset_text_query.name) {
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    GetIconForPresetQueryCategory(preset_text_query.category),
                    ui::kColorSysOnSurface, kIconSizeDip));

  SetLabelStyle(views::style::STYLE_BODY_4_EMPHASIS);
  SetTextColorId(ButtonState::STATE_NORMAL, ui::kColorSysOnSurface);
  SetImageLabelSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_VECTOR_ICON_PADDING));
  SetCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh));
  SetCustomPadding(kChipInsets);
}

EditorMenuChipView::~EditorMenuChipView() = default;

BEGIN_METADATA(EditorMenuChipView)
END_METADATA

}  // namespace chromeos::editor_menu
