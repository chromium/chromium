// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/mute_button.h"

#include <string_view>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace voice {

namespace {

const gfx::VectorIcon& GetIcon(bool muted) {
  if (muted) {
    return features::IsRoundedIconsEnabled()
               ? vector_icons::kMicOffIcon
               : vector_icons::kMicOffChromeRefreshOldIcon;
  }
  return features::IsRoundedIconsEnabled()
             ? vector_icons::kMicIcon
             : vector_icons::kMicChromeRefreshOldIcon;
}

}  // namespace

MuteButton::MuteButton(PressedCallback callback,
                       std::u16string mute_label,
                       std::u16string unmute_label)
    : views::MdTextButton(std::move(callback),
                          std::u16string_view(),
                          views::style::CONTEXT_BUTTON_MD,
                          /*use_text_color_for_icon=*/false),
      mute_label_(std::move(mute_label)),
      unmute_label_(std::move(unmute_label)) {
  const ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  SetStyle(ui::ButtonStyle::kProminent);
  SetAppearDisabledInInactiveWidget(false);
  SetMinSize(gfx::Size(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE) +
          2 * lp->GetDistanceMetric(views::DISTANCE_BUTTON_HORIZONTAL_PADDING),
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
  Update();
}

MuteButton::~MuteButton() = default;

void MuteButton::SetMuted(bool muted) {
  if (muted_ == muted) {
    return;
  }
  muted_ = muted;
  Update();
}

void MuteButton::Update() {
  const gfx::VectorIcon& icon = GetIcon(muted_);
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_ICON_SIZE);
  SetImageModel(STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    icon, ui::kColorButtonForegroundProminent, icon_size));
  SetImageModel(STATE_DISABLED,
                ui::ImageModel::FromVectorIcon(
                    icon, ui::kColorButtonForegroundDisabled, icon_size));
  const std::u16string& label = muted_ ? unmute_label_ : mute_label_;
  SetAccessibleName(label);
  SetTooltipText(label);
}

BEGIN_METADATA(MuteButton)
END_METADATA

}  // namespace voice
