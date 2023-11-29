// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"

#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "media/base/video_transformation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"

VideoStreamView::VideoStreamView(float default_aspect_ratio)
    : current_aspect_ratio_(default_aspect_ratio),
      rounded_radius_(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius)) {
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MEDIA_PREVIEW_VIDEO_STREAM_ACCESSIBLE_NAME));
  SetAccessibleRole(ax::mojom::Role::kImage);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

VideoStreamView::~VideoStreamView() {
  ClearFrame();
}

void VideoStreamView::ScheduleFramePaint(
    scoped_refptr<media::VideoFrame> frame) {
  latest_frame_ = std::move(frame);

  if (!has_updated_preferred_size_) {
    if (latest_frame_) {
      gfx::Size frame_size = latest_frame_->natural_size();
      current_aspect_ratio_ =
          frame_size.width() / static_cast<float>(frame_size.height());
    }
    PreferredSizeChanged();
    has_updated_preferred_size_ = true;
  }

  SchedulePaint();
}

void VideoStreamView::ClearFrame() {
  has_updated_preferred_size_ = false;
  video_renderer_.ResetCache();
  latest_frame_.reset();
  PreferredSizeChanged();
  SchedulePaint();
}

void VideoStreamView::OnPaint(gfx::Canvas* canvas) {
  if (!latest_frame_) {
    gfx::RectF base_rect(width(), height());
    canvas->DrawRoundRect(base_rect, rounded_radius_, cc::PaintFlags());
    return;
  }

  if (features::IsChromeRefresh2023()) {
    canvas->sk_canvas()->clipRRect(
        SkRRect::MakeRectXY(SkRect::MakeIWH(width(), height()), rounded_radius_,
                            rounded_radius_),
        /*do_anti_alias=*/true);
  }
  const gfx::RectF dest_rect(width(), height());
  cc::PaintFlags flags;
  media::VideoTransformation transformation;
  transformation.mirrored = true;
  video_renderer_.Paint(std::move(latest_frame_), canvas->sk_canvas(),
                        dest_rect, flags, transformation,
                        raster_context_provider_.get());
}

int VideoStreamView::GetHeightForWidth(int w) const {
  return w / current_aspect_ratio_;
}

gfx::Size VideoStreamView::CalculatePreferredSize() const {
  return gfx::Size(width(), GetHeightForWidth(width()));
}

void VideoStreamView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  has_updated_preferred_size_ = false;
}

BEGIN_METADATA(VideoStreamView)
END_METADATA
