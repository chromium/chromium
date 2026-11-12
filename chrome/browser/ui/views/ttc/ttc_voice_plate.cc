// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/ttc/ttc_view_delegate.h"
#include "chrome/browser/ui/views/voice/mute_button.h"
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

using voice::MuteButton;
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
      return WaveformMode::kLive;
  }
  NOTREACHED();
}

// What screen readers report about the session in `state`.
std::u16string GetAccessibleStatus(UiState state) {
  switch (state) {
    case UiState::kInitializing:
      return l10n_util::GetStringUTF16(
          IDS_TTC_VOICE_PLATE_INITIALIZING_ACCNAME);
    case UiState::kLive:
      return l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_LISTENING_ACCNAME);
    case UiState::kMicMuted:
      return l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_MIC_MUTED_ACCNAME);
  }
  NOTREACHED();
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kViewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kWaveformElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcVoicePlate, kMuteButtonElementId);
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

  mute_button_ = contents->AddChildView(std::make_unique<MuteButton>(
      base::BindRepeating(&TtcViewDelegate::ToggleMicMuted,
                          base::Unretained(&delegate)),
      l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_MUTE),
      l10n_util::GetStringUTF16(IDS_TTC_VOICE_PLATE_UNMUTE)));
  mute_button_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0,
          lp->GetDistanceMetric(
              DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING),
          0, 0));
  mute_button_->SetProperty(views::kElementIdentifierKey, kMuteButtonElementId);

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
  // it on every move; ApplyUiState() announces when the state changes.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);

  SetContentsView(std::move(contents));
  CreateWidget();

  audio_level_subscription_ =
      delegate.RegisterAudioLevelCallback(base::BindRepeating(
          &TtcVoicePlate::UpdateAudioLevel, base::Unretained(this)));
}

TtcVoicePlate::~TtcVoicePlate() = default;

void TtcVoicePlate::ApplyUiState(UiState state) {
  waveform_view_->SetMode(ToWaveformMode(state));
  mute_button_->SetMuted(state == UiState::kMicMuted);
  mute_button_->SetEnabled(state != UiState::kInitializing);

  const std::u16string status = GetAccessibleStatus(state);
  views::ViewAccessibility& waveform_accessibility =
      waveform_view_->GetViewAccessibility();
  if (status == waveform_accessibility.GetCachedName()) {
    return;
  }
  waveform_accessibility.SetName(status, ax::mojom::NameFrom::kAttribute);

  // A plate recreated in another window gets its state before it is shown;
  // only announce when the user can see the change.
  if (GetWidget() && GetWidget()->IsVisible()) {
    waveform_accessibility.AnnouncePolitely(status);
  }
}

void TtcVoicePlate::Show() {
  VoicePlate::Show();
}

void TtcVoicePlate::UpdateAudioLevel(float audio_level) {
  waveform_view_->SetAudioLevel(audio_level);
}

}  // namespace ttc
