// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/typography.h"

namespace chromeos::editor_menu {

namespace {

constexpr int kHeightDip = 32;
constexpr int kHorizontalPaddingDip = 8;
constexpr int kIconSizeDip = 20;
constexpr int kImageLabelSpacingDip = 8;
constexpr int kRadiusDip = 8;
constexpr int kBorderThicknessDip = 1;

}  // namespace

EditorMenuChipView::EditorMenuChipView(views::Button::PressedCallback callback,
                                       const std::u16string& text,
                                       const gfx::VectorIcon* icon)
    : views::LabelButton(std::move(callback), text), icon_(icon) {
  CHECK(icon_);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetBaseColorId(ui::kColorIcon);
  SetHasInkDropActionOnClick(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), kRadiusDip));

  SetTooltipText(text);
  SetImageLabelSpacing(kImageLabelSpacingDip);
}

EditorMenuChipView::~EditorMenuChipView() = default;

void EditorMenuChipView::AddedToWidget() {
  // Only initialize the button after the button is added to a widget.
  InitLayout();
}

gfx::Size EditorMenuChipView::CalculatePreferredSize() const {
  int width = 0;

  // Add the padding at both sides.
  width += 2 * kHorizontalPaddingDip;

  // Add the icon width and the spacing between the icon and the text.
  width += kIconSizeDip + GetImageLabelSpacing();

  // Add the text width.
  width += label()->GetPreferredSize().width();

  gfx::Size size(width, kHeightDip);
  return size;
}

void EditorMenuChipView::InitLayout() {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);
  label()->SetEnabledColorId(ui::kColorSysOnSurface);
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    *icon_, cros_tokens::kCrosSysPrimary, kIconSizeDip));
  SetBorder(views::CreateThemedRoundedRectBorder(
      kBorderThicknessDip, kRadiusDip, ui::kColorSysTonalOutline));
  PreferredSizeChanged();
}

BEGIN_METADATA(EditorMenuChipView, views::LabelButton)
END_METADATA

}  // namespace chromeos::editor_menu
