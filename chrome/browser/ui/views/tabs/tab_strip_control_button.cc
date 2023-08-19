// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"

const int TabStripControlButton::kIconSize = 16;
const gfx::Size TabStripControlButton::kButtonSize{28, 28};

TabStripControlButton::TabStripControlButton(TabStrip* tab_strip,
                                             PressedCallback callback,
                                             const gfx::VectorIcon& icon)
    : views::LabelButton(std::move(callback)),
      icon_(icon),
      tab_strip_(tab_strip) {
  SetImageCentered(true);

  paint_transparent_for_custom_image_theme_ = true;
  foreground_frame_active_color_id_ = kColorTabForegroundInactiveFrameActive;
  foreground_frame_inactive_color_id_ =
      kColorNewTabButtonCRForegroundFrameInactive;
  background_frame_active_color_id_ = kColorNewTabButtonBackgroundFrameActive;
  background_frame_inactive_color_id_ =
      kColorNewTabButtonBackgroundFrameInactive;

  UpdateIcon();
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  SetFocusRingCornerRadius(28);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  if (features::IsChromeRefresh2023()) {
    views::InkDrop::Get(this)->SetLayerRegion(views::LayerRegion::kAbove);
  }
  UpdateInkDrop();
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
}

ui::ColorId TabStripControlButton::GetBackgroundColor() {
  return (GetWidget() && GetWidget()->ShouldPaintAsActive())
             ? background_frame_active_color_id_
             : background_frame_inactive_color_id_;
}

ui::ColorId TabStripControlButton::GetForegroundColor() {
  return (GetWidget() && GetWidget()->ShouldPaintAsActive())
             ? foreground_frame_active_color_id_
             : foreground_frame_inactive_color_id_;
}

void TabStripControlButton::UpdateIcon() {
  const ui::ImageModel icon_image_model = ui::ImageModel::FromVectorIcon(
      icon_.get(), GetForegroundColor(), kIconSize);

  SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  SetImageModel(views::Button::STATE_HOVERED, icon_image_model);
  SetImageModel(views::Button::STATE_PRESSED, icon_image_model);
}

void TabStripControlButton::UpdateInkDrop() {
  const auto* const color_provider = GetColorProvider();

  if (!color_provider) {
    return;
  }

  if (features::IsChromeRefresh2023()) {
    return CreateToolbarInkdropCallbacks(
        this, kColorTabStripControlButtonInkDrop,
        kColorTabStripControlButtonInkDropRipple);

  } else {
    const bool frame_active =
        (GetWidget() && GetWidget()->ShouldPaintAsActive());

    // These values are also used in refresh by
    // `kColorTabStripControlButtonInkDrop` and
    // `kColorTabStripControlButtonInkDropRipple` in case of themes.
    views::InkDrop::Get(this)->SetHighlightOpacity(0.16f);
    views::InkDrop::Get(this)->SetVisibleOpacity(0.14f);
    views::InkDrop::Get(this)->SetBaseColor(color_provider->GetColor(
        frame_active ? kColorNewTabButtonInkDropFrameActive
                     : kColorNewTabButtonInkDropFrameInactive));
  }
}

void TabStripControlButton::UpdateColors() {
  const auto* const color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  UpdateBackground();
  UpdateInkDrop();
  UpdateIcon();
  SchedulePaint();
}

void TabStripControlButton::UpdateBackground() {
  const absl::optional<int> bg_id =
      tab_strip_->GetCustomBackgroundId(BrowserFrameActiveState::kUseCurrent);

  // Paint the background as transparent for image based themes.
  if (bg_id.has_value() && paint_transparent_for_custom_image_theme_) {
    SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  } else {
    SetBackground(views::CreateThemedRoundedRectBackground(GetBackgroundColor(),
                                                           GetCornerRadius()));
  }
}

int TabStripControlButton::GetCornerRadius() {
  return width() / 2;
}

void TabStripControlButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &TabStripControlButton::UpdateColors, base::Unretained(this)));
  UpdateColors();
}

void TabStripControlButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void TabStripControlButton::OnThemeChanged() {
  views::LabelButton::OnThemeChanged();
  UpdateColors();
}

gfx::Size TabStripControlButton::CalculatePreferredSize() const {
  gfx::Size size = TabStripControlButton::kButtonSize;
  return size;
}

void TabStripControlButton::NotifyClick(const ui::Event& event) {
  LabelButton::NotifyClick(event);
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTION_TRIGGERED);
}

void TabStripControlButton::AnimateToStateForTesting(
    views::InkDropState state) {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(state);
}

BEGIN_METADATA(TabStripControlButton, views::LabelButton)
END_METADATA
