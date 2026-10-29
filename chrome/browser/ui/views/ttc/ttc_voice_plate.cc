// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ttc {

using voice::WaveformMode;
using voice::WaveformView;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kViewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kWaveformElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kCloseButtonElementId);

TtcVoicePlate::TtcVoicePlate(views::View* anchor_view,
                             base::RepeatingClosure close_callback)
    : VoicePlate(anchor_view) {
  const ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();

  auto contents = std::make_unique<views::View>();
  contents->SetProperty(views::kElementIdentifierKey, kViewElementId);
  contents->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetMinimumCrossAxisSize(
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON));

  waveform_view_ = contents->AddChildView(
      std::make_unique<WaveformView>(/*full_size=*/true));
  waveform_view_->SetProperty(views::kElementIdentifierKey, kWaveformElementId);

  views::ImageButton* close_button =
      contents->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          std::move(close_callback),
          features::IsRoundedIconsEnabled()
              ? vector_icons::kCloseIcon
              : vector_icons::kCloseChromeRefreshOldIcon,
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE),
          ui::kColorSysOnSurface, ui::kColorIconDisabled,
          ui::kColorSysOnSurface));
  close_button->SetBorder(views::CreateEmptyBorder(
      lp->GetInsetsMetric(views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON)));
  views::InstallCircleHighlightPathGenerator(close_button);
  close_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING),
          0, 0));
  close_button->SetProperty(views::kElementIdentifierKey,
                            kCloseButtonElementId);

  SetContentsView(std::move(contents));
  CreateWidget();
}

TtcVoicePlate::~TtcVoicePlate() = default;

void TtcVoicePlate::SetMode(WaveformMode mode) {
  waveform_view_->SetMode(mode);
}

void TtcVoicePlate::UpdateAudioLevel(float audio_level) {
  waveform_view_->SetAudioLevel(audio_level);
}

}  // namespace ttc
