// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/back_to_tab_label_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kBackToTabButtonMargin = 48;

constexpr int kBackToTabButtonSize = 20;

constexpr int kBackToTabImageSize = 14;

constexpr int kBackToTabBorderThickness = 8;

constexpr int kBackToTabBorderRadius =
    (kBackToTabButtonSize + (2 * kBackToTabBorderThickness)) / 2;

}  // namespace

BackToTabLabelButton::BackToTabLabelButton(PressedCallback callback)
    : LabelButton(std::move(callback)) {
  SetMinSize(gfx::Size(kBackToTabButtonSize, kBackToTabButtonSize));
  SetMaxSize(gfx::Size(kBackToTabButtonSize, kBackToTabButtonSize));

  // Keep the image to the right of the text.
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);

  // Elide the origin at the front of the text.
  SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);

  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          views::kLaunchIcon, kColorPipWindowForeground, kBackToTabImageSize));

  // Prevent DCHECKing for our non-opaque background.
  SetTextSubpixelRenderingEnabled(false);

  SetBorder(views::CreateEmptyBorder(kBackToTabBorderThickness));

  const std::u16string back_to_tab_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT));
  SetText(back_to_tab_button_label);

  // Accessibility.
  GetViewAccessibility().SetName(back_to_tab_button_label);
  SetInstallFocusRingOnFocus(true);
}

BackToTabLabelButton::~BackToTabLabelButton() = default;

ui::Cursor BackToTabLabelButton::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kHand;
}

void BackToTabLabelButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(kColorPipWindowBackToTabButtonBackground),
      kBackToTabBorderRadius));
  SetEnabledTextColorIds(kColorPipWindowForeground);
  SetTextColorId(views::Button::STATE_DISABLED, kColorPipWindowForeground);
}

void BackToTabLabelButton::SetWindowSize(const gfx::Size& window_size) {
  if (window_size_.has_value() && window_size_.value() == window_size)
    return;

  window_size_ = window_size;
  UpdateSizingAndPosition();
}

void BackToTabLabelButton::UpdateSizingAndPosition() {
  if (!window_size_.has_value())
    return;

  SetMaxSize(gfx::Size(window_size_->width() - kBackToTabButtonMargin,
      kBackToTabButtonSize));
  SetSize(CalculatePreferredSize({}));
  LabelButton::SetPosition(
      gfx::Point((window_size_->width() / 2) - (size().width() / 2),
                 (window_size_->height() / 2) - (size().height() / 2)));
}

BEGIN_METADATA(BackToTabLabelButton)
END_METADATA
