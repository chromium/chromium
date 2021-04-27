// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/back_to_tab_label_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kBackToTabButtonSize = 20;

constexpr int kBackToTabImageSize = 14;

constexpr int kBackToTabBorderThickness = 8;

constexpr int kBackToTabBorderRadius =
    (kBackToTabButtonSize + (2 * kBackToTabBorderThickness)) / 2;

constexpr SkColor kBackToTabBackgroundColor = SkColorSetA(SK_ColorBLACK, 0x60);

}  // namespace

BackToTabLabelButton::BackToTabLabelButton(PressedCallback callback)
    : LabelButton(std::move(callback)) {
  SetMinSize(gfx::Size(kBackToTabButtonSize, kBackToTabButtonSize));
  SetMaxSize(gfx::Size(kBackToTabButtonSize, kBackToTabButtonSize));

  // Keep the image to the right of the text.
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);

  // Elide the origin at the front of the text.
  SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(views::kLaunchIcon, kBackToTabImageSize,
                                 kPipWindowIconColor));

  // Prevent DCHECKing for our non-opaque background.
  SetTextSubpixelRenderingEnabled(false);

  // Leave the border transparent since the background will color it.
  SetBorder(views::CreateRoundedRectBorder(
      kBackToTabBorderThickness, kBackToTabBorderRadius, SK_ColorTRANSPARENT));

  SetBackground(views::CreateRoundedRectBackground(kBackToTabBackgroundColor,
                                                   kBackToTabBorderRadius));

  SetEnabledTextColors(kPipWindowTextColor);
  SetTextColor(views::Button::STATE_DISABLED, kPipWindowTextColor);

  // Accessibility.
  const std::u16string back_to_tab_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT));
  SetAccessibleName(back_to_tab_button_label);
  SetTooltipText(back_to_tab_button_label);
  SetInstallFocusRingOnFocus(true);
}

BackToTabLabelButton::~BackToTabLabelButton() = default;

void BackToTabLabelButton::SetText(const std::u16string& text) {
  views::LabelButton::SetText(text);
  UpdateSizingAndPosition();
}

void BackToTabLabelButton::SetWindowSize(const gfx::Size& window_size) {
  if (window_size_.has_value() && window_size_.value() == window_size)
    return;

  window_size_ = window_size;
  UpdateSizingAndPosition();
}

bool BackToTabLabelButton::IsTextElidedForTesting() {
  return label()->IsDisplayTextTruncated();
}

void BackToTabLabelButton::UpdateSizingAndPosition() {
  if (!window_size_.has_value())
    return;

  SetMaxSize(gfx::Size(window_size_->width() - 48, kBackToTabButtonSize));
  SetSize(CalculatePreferredSize());
  LabelButton::SetPosition(
      gfx::Point((window_size_->width() / 2) - (size().width() / 2),
                 (window_size_->height() / 2) - (size().height() / 2)));
}

BEGIN_METADATA(BackToTabLabelButton, views::LabelButton)
END_METADATA
