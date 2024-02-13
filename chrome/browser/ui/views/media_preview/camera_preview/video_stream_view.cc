// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"

#include <algorithm>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/context_factory.h"
#include "media/base/video_transformation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"

VideoStreamView::VideoStreamView()
    : current_aspect_ratio_(video_format_comparison::kDefaultAspectRatio),
      rounded_radius_(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)) {
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MEDIA_PREVIEW_VIDEO_STREAM_ACCESSIBLE_NAME));
  SetAccessibleRole(ax::mojom::Role::kImage);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  raster_context_provider_ =
      content::GetContextFactory()->SharedMainThreadRasterContextProvider();
  if (raster_context_provider_) {
    raster_context_provider_->AddObserver(this);
  }
}

VideoStreamView::~VideoStreamView() {
  ClearFrame();
  if (raster_context_provider_) {
    raster_context_provider_->RemoveObserver(this);
  }
}

void VideoStreamView::OnContextLost() {
  if (raster_context_provider_) {
    raster_context_provider_->RemoveObserver(this);
  }

  raster_context_provider_ =
      content::GetContextFactory()->SharedMainThreadRasterContextProvider();
  if (raster_context_provider_) {
    raster_context_provider_->AddObserver(this);
  }
}

void VideoStreamView::ScheduleFramePaint(
    scoped_refptr<media::VideoFrame> frame) {
  latest_frame_ = std::move(frame);

  if (!has_updated_preferred_size_) {
    if (latest_frame_) {
      // Caps the height to keep vertical videos from taking up too much
      // vertical space.
      current_aspect_ratio_ =
          std::max(video_format_comparison::kMinAspectRatio,
                   video_format_comparison::GetFrameAspectRatio(
                       latest_frame_->natural_size()));
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
    gfx::RectF background_rect(width(), height());
    cc::PaintFlags background_flags;
    background_flags.setAntiAlias(true);
    canvas->DrawRoundRect(background_rect, rounded_radius_, background_flags);
    return;
  }

  // Centers the video frame horizontally in the view
  int rendered_frame_width =
      height() * video_format_comparison::GetFrameAspectRatio(
                     latest_frame_->natural_size());
  float x = (width() - rendered_frame_width) / 2.0;

  if (features::IsChromeRefresh2023()) {
    canvas->sk_canvas()->clipRRect(
        SkRRect::MakeRectXY(
            SkRect::MakeXYWH(x, 0, rendered_frame_width, height()),
            rounded_radius_, rounded_radius_),
        /*do_anti_alias=*/true);
  }

  const gfx::RectF dest_rect(x, 0, rendered_frame_width, height());
  cc::PaintFlags flags;
  // Select high quality frame scaling.
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kHigh);
  flags.setAntiAlias(true);
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
