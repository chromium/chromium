// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/ttc/ttc_view_delegate.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
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

namespace {

WaveformMode ToWaveformMode(UiState state) {
  switch (state) {
    case UiState::kInitializing:
      return WaveformMode::kProcessing;
    case UiState::kMicMuted:
      return WaveformMode::kIdle;
    case UiState::kLive:
    default:
      return WaveformMode::kLive;
  }
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kViewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kWaveformElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kCloseButtonElementId);

TtcVoicePlate::TtcVoicePlate(views::View* anchor_view,
                             TtcViewDelegate& delegate)
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
  // Screen readers report the session state through the waveform.
  waveform_view_->GetViewAccessibility().SetRole(ax::mojom::Role::kStatus);

  views::ImageButton* close_button =
      contents->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&TtcViewDelegate::EndSessionAsync,
                              base::Unretained(&delegate)),
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

  SetAccessibleTitle(l10n_util::GetStringUTF16(IDS_TTC_ENTRYPOINT_LABEL));
  // The plate is recreated each time it moves to another window. An alert
  // dialog (the default for bubbles without a focused view) would re-announce
  // it on every move; ApplyUiState() announces when listening starts instead.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);

  SetContentsView(std::move(contents));
  CreateWidget();

  audio_level_subscription_ =
      delegate.RegisterAudioLevelCallback(base::BindRepeating(
          &TtcVoicePlate::UpdateAudioLevel, base::Unretained(this)));
}

TtcVoicePlate::~TtcVoicePlate() = default;

void TtcVoicePlate::ApplyUiState(UiState state) {
  const WaveformMode mode = ToWaveformMode(state);
  if (mode == waveform_view_->mode()) {
    return;
  }
  waveform_view_->SetMode(mode);

  std::u16string name;
  switch (mode) {
    case WaveformMode::kProcessing:
      name = l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_PROCESSING_ACCNAME);
      break;
    case WaveformMode::kLive:
      name = l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_LISTENING_ACCNAME);
      break;
    case WaveformMode::kIdle:
    case WaveformMode::kWaiting:
      break;
  }
  views::ViewAccessibility& waveform_accessibility =
      waveform_view_->GetViewAccessibility();
  waveform_accessibility.SetName(
      name, name.empty() ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::NameFrom::kAttribute);

  // A plate recreated in another window is set to kLive before it is shown;
  // only announce listening when the user can see the change.
  if (mode == WaveformMode::kLive && GetWidget() && GetWidget()->IsVisible()) {
    waveform_accessibility.AnnouncePolitely(name);
  }
}

void TtcVoicePlate::Show() {
  VoicePlate::Show();
}

void TtcVoicePlate::UpdateAudioLevel(float audio_level) {
  waveform_view_->SetAudioLevel(audio_level);
}

}  // namespace ttc
