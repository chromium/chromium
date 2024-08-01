// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_expand_button.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/style/ash_color_provider.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace sharesheet {

SharesheetExpandButton::SharesheetExpandButton(PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(kExpandButtonInsideBorderInsetsVertical,
                      kExpandButtonInsideBorderInsetsHorizontal),
      kExpandButtonBetweenChildSpacing, true));
  // Sets all views to be center-aligned along the orientation axis.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<views::ImageView>());

  label_ = AddChildView(
      CreateShareLabel(std::u16string(), TypographyToken::kCrosButton2,
                       cros_tokens::kCrosSysPrimary, gfx::ALIGN_CENTER));
  SetFocusBehavior(View::FocusBehavior::ALWAYS);
  SetToDefaultState();
}

void SharesheetExpandButton::SetToDefaultState() {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kCaretDownIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorProminent),
      kExpandButtonCaretIconSize));
  auto display_name = l10n_util::GetStringUTF16(IDS_SHARESHEET_MORE_APPS_LABEL);
  label_->SetText(display_name);
  GetViewAccessibility().SetName(display_name);
}

void SharesheetExpandButton::SetToExpandedState() {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kCaretUpIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorProminent),
      kExpandButtonCaretIconSize));
  auto display_name =
      l10n_util::GetStringUTF16(IDS_SHARESHEET_FEWER_APPS_LABEL);
  label_->SetText(display_name);
  GetViewAccessibility().SetName(display_name);
}

BEGIN_METADATA(SharesheetExpandButton)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
