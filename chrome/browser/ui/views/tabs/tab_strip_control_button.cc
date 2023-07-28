// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
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
    : views::LabelButton(std::move(callback)), icon_(icon) {
  SetImageCentered(true);
  bool is_chrome_refresh = features::IsChromeRefresh2023();

  if (is_chrome_refresh) {
    foreground_frame_active_color_id_ =
        kColorNewTabButtonCRForegroundFrameActive;
    foreground_frame_inactive_color_id_ =
        kColorNewTabButtonCRForegroundFrameInactive;
    background_frame_active_color_id_ =
        kColorNewTabButtonCRBackgroundFrameActive;
    background_frame_inactive_color_id_ =
        kColorNewTabButtonCRBackgroundFrameInactive;
  } else {
    foreground_frame_active_color_id_ = kColorNewTabButtonForegroundFrameActive;
    foreground_frame_inactive_color_id_ =
        kColorNewTabButtonForegroundFrameInactive;
    background_frame_active_color_id_ = kColorNewTabButtonBackgroundFrameActive;
    background_frame_inactive_color_id_ =
        kColorNewTabButtonBackgroundFrameInactive;
  }

  UpdateIcon();
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  SetFocusRingCornerRadius(28);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  const auto* const color_provider = GetColorProvider();

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetHighlightOpacity(is_chrome_refresh ? 1.0f
                                                                   : 0.16f);
  // TODO (1399942) Decide on changes to visible opacity for chrome refresh
  views::InkDrop::Get(this)->SetVisibleOpacity(0.14f);
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);

  if (color_provider) {
    views::InkDrop::Get(this)->SetBaseColor(color_provider->GetColor(
        GetWidget()->ShouldPaintAsActive()
            ? (is_chrome_refresh ? kColorTabBackgroundInactiveHoverFrameActive
                                 : kColorNewTabButtonInkDropFrameActive)
            : (is_chrome_refresh ? kColorTabBackgroundInactiveHoverFrameInactive
                                 : kColorNewTabButtonInkDropFrameInactive)));
  }
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

void TabStripControlButton::UpdateColors() {
  const auto* const color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  const bool frame_active = (GetWidget() && GetWidget()->ShouldPaintAsActive());
  const ui::ColorId hover_color =
      features::IsChromeRefresh2023()
          ? (frame_active ? kColorTabBackgroundInactiveHoverFrameActive
                          : kColorTabBackgroundInactiveHoverFrameInactive)
          : (frame_active ? kColorNewTabButtonInkDropFrameActive
                          : kColorNewTabButtonInkDropFrameInactive);

  views::InkDrop::Get(this)->SetBaseColor(
      color_provider->GetColor(hover_color));
  UpdateIcon();
  SchedulePaint();
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
