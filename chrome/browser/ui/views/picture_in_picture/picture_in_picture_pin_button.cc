// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_pin_button.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kPinButtonIconSize = 16;

}  // namespace

// static
bool PictureInPicturePinButton::IsEnabled() {
  return ash::features::IsPipAutoLayoutToggleEnabled();
}

PictureInPicturePinButton::PictureInPicturePinButton()
    : views::ToggleImageButton(
          base::BindRepeating(&PictureInPicturePinButton::OnPressed,
                              base::Unretained(this))) {
  const bool rounded_icons = features::IsRoundedIconsEnabled();
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    rounded_icons ? kKeepIcon : kKeepOldIcon,
                    kColorPipWindowForeground, kPinButtonIconSize));
  SetToggledImageModel(views::Button::STATE_NORMAL,
                       ui::ImageModel::FromVectorIcon(
                           rounded_icons ? kKeepOffIcon : kKeepOffOldIcon,
                           kColorPipWindowForeground, kPinButtonIconSize));

  const std::u16string pin_label =
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PIN_WINDOW_TEXT);
  const std::u16string unpin_label =
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_UNPIN_WINDOW_TEXT);
  SetTooltipText(pin_label);
  GetViewAccessibility().SetName(pin_label);
  SetToggledTooltipText(unpin_label);
  SetToggledAccessibleName(unpin_label);

  // Match the style of the other PiP window control buttons.
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  views::ConfigureVectorImageButton(this);
  views::InkDrop::Get(this)->SetBaseColor(kColorPipWindowForeground);
  views::InstallCircleHighlightPathGenerator(this);
  SetInstallFocusRingOnFocus(true);
}

PictureInPicturePinButton::~PictureInPicturePinButton() = default;

ui::Cursor PictureInPicturePinButton::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kHand;
}

void PictureInPicturePinButton::AddedToWidget() {
  views::ToggleImageButton::AddedToWidget();
  window_observation_.Observe(GetWidget()->GetNativeWindow());
  UpdateToggled();
}

void PictureInPicturePinButton::RemovedFromWidget() {
  window_observation_.Reset();
  views::ToggleImageButton::RemovedFromWidget();
}

void PictureInPicturePinButton::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  if (key == ash::kPipParamsKey) {
    UpdateToggled();
  }
}

void PictureInPicturePinButton::OnWindowDestroying(aura::Window* window) {
  window_observation_.Reset();
}

void PictureInPicturePinButton::OnPressed() {
  aura::Window* window = GetWidget()->GetNativeWindow();
  const ash::PipParams* current = window->GetProperty(ash::kPipParamsKey);
  ash::PipParams params = current ? *current : ash::PipParams{};
  params.auto_layout = !params.auto_layout;
  window->SetProperty(ash::kPipParamsKey, params);
}

void PictureInPicturePinButton::UpdateToggled() {
  const ash::PipParams* params =
      GetWidget()->GetNativeWindow()->GetProperty(ash::kPipParamsKey);
  const bool auto_layout = !params || params->auto_layout;
  SetToggled(!auto_layout);
}

BEGIN_METADATA(PictureInPicturePinButton)
END_METADATA
