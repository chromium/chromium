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
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"

VideoStreamView::VideoStreamView()
    : targeted_aspect_ratio_(video_format_comparison::kDefaultAspectRatio),
      rounded_radius_(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)),
      preview_base_color_(SK_ColorBLACK) {
  SetAccessibilityProperties(
      ax::mojom::Role::kImage,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_PREVIEW_VIDEO_STREAM_ACCESSIBLE_NAME));

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
  SchedulePaint();
}

void VideoStreamView::ClearFrame() {
  video_renderer_.ResetCache();
  latest_frame_.reset();
  rendered_frame_count_ = 0;
  PreferredSizeChanged();
  SchedulePaint();
}

size_t VideoStreamView::GetRenderedFrameCount() {
  return rendered_frame_count_;
}

void VideoStreamView::OnPaint(gfx::Canvas* canvas) {
  const auto background_rect = SkRect::MakeWH(width(), height());
  canvas->sk_canvas()->clipRRect(
      SkRRect::MakeRectXY(background_rect, rounded_radius_, rounded_radius_),
      /*do_anti_alias=*/true);

  cc::PaintFlags background_flags;
  background_flags.setAntiAlias(true);
  background_flags.setColor(preview_base_color_);
  canvas->sk_canvas()->drawRect(background_rect, background_flags);

  if (!latest_frame_) {
    return;
  }

  ++rendered_frame_count_;

  int rendered_frame_width = width();
  int rendered_frame_height = height();
  float x = 0;
  float y = 0;

  float frame_aspect_ratio = video_format_comparison::GetFrameAspectRatio(
      latest_frame_->natural_size());

  if (frame_aspect_ratio < targeted_aspect_ratio_) {
    // Centers the video frame horizontally in the view.
    rendered_frame_width = height() * frame_aspect_ratio;
    x = (width() - rendered_frame_width) / 2.0;
  } else {
    // Centers the video frame vertically in the view.
    rendered_frame_height = width() / frame_aspect_ratio;
    y = (height() - rendered_frame_height) / 2.0;
  }

  const gfx::RectF dest_rect(x, y, rendered_frame_width, rendered_frame_height);
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
  return w / targeted_aspect_ratio_;
}

gfx::Size VideoStreamView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  return gfx::Size(width(), GetHeightForWidth(width()));
}

void VideoStreamView::OnThemeChanged() {
  views::View::OnThemeChanged();
  preview_base_color_ = GetColorProvider()->GetColor(ui::kColorSysSurface2);
}

BEGIN_METADATA(VideoStreamView)
END_METADATA
