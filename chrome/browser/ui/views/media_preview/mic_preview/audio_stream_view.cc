// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/audio_stream_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"

AudioStreamView::AudioStreamView()
    : rounded_radius_(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius)) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_MEDIA_PREVIEW_AUDIO_STREAM_ACCESSIBLE_NAME));
  SetFlipCanvasOnPaintForRTLUI(true);
}

AudioStreamView::~AudioStreamView() = default;

void AudioStreamView::ScheduleAudioStreamPaint(float audio_value) {
  last_audio_level_ = audio_value;
  SchedulePaint();
}

void AudioStreamView::Clear() {
  last_audio_level_ = 0;
  SchedulePaint();
}

void AudioStreamView::OnPaint(gfx::Canvas* canvas) {
  const float rect_height = height() / 2;
  const float x = 0;
  const float y = rect_height / 2;

  gfx::RectF base_rect(x, y, width(), rect_height);
  cc::PaintFlags base_rect_flags;
  base_rect_flags.setColor(GetColorProvider()->GetColor(ui::kColorSysSurface5));
  base_rect_flags.setAntiAlias(true);
  canvas->DrawRoundRect(base_rect, rounded_radius_, base_rect_flags);

  if (last_audio_level_ != 0) {
    float rect_width = width() * last_audio_level_;
    // Ensure that the smallest possible colored bar is a circle.
    rect_width = std::max(rect_width, rect_height);

    gfx::RectF value_rect(x, y, rect_width, rect_height);
    cc::PaintFlags value_rect_flags;
    value_rect_flags.setColor(
        GetColorProvider()->GetColor(ui::kColorSysPrimary));
    value_rect_flags.setAntiAlias(true);
    canvas->DrawRoundRect(value_rect, rounded_radius_, value_rect_flags);
  }
}

BEGIN_METADATA(AudioStreamView)
END_METADATA
