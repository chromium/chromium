// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_action_button.h"

#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace global_media_controls {

namespace {

using media_session::mojom::MediaSessionAction;

}  // namespace

MediaActionButton::MediaActionButton(PressedCallback callback,
                                     int button_id,
                                     int tooltip_text_id,
                                     int icon_size,
                                     const gfx::VectorIcon& vector_icon,
                                     gfx::Size button_size,
                                     ui::ColorId foreground_color_id,
                                     ui::ColorId foreground_disabled_color_id,
                                     ui::ColorId focus_ring_color_id)
    : ImageButton(std::move(callback)),
      icon_size_(icon_size),
      foreground_disabled_color_id_(foreground_disabled_color_id) {
  views::ConfigureVectorImageButton(this);

  bool enable_flip_canvas =
      (button_id == static_cast<int>(MediaSessionAction::kPreviousTrack) ||
       button_id == static_cast<int>(MediaSessionAction::kNextTrack));
  SetFlipCanvasOnPaintForRTLUI(enable_flip_canvas);

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                button_size.height() / 2);
  SetPreferredSize(button_size);

  SetInstallFocusRingOnFocus(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetColorId(focus_ring_color_id);

  Update(button_id, vector_icon, tooltip_text_id, foreground_color_id);
}

MediaActionButton::~MediaActionButton() = default;

void MediaActionButton::Update(int button_id,
                               const gfx::VectorIcon& vector_icon,
                               int tooltip_text_id,
                               ui::ColorId foreground_color_id) {
  if (button_id != kEmptyMediaActionButtonId) {
    SetID(button_id);
  }
  foreground_color_id_ = foreground_color_id;
  SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  views::SetImageFromVectorIconWithColorId(
      this, vector_icon, foreground_color_id_, foreground_disabled_color_id_,
      icon_size_);
}

void MediaActionButton::UpdateText(int tooltip_text_id) {
  SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
}

void MediaActionButton::UpdateIcon(const gfx::VectorIcon& vector_icon) {
  views::SetImageFromVectorIconWithColorId(
      this, vector_icon, foreground_color_id_, foreground_disabled_color_id_,
      icon_size_);
}

BEGIN_METADATA(MediaActionButton)
END_METADATA

}  // namespace global_media_controls
