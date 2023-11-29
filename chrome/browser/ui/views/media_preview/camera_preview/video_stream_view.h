// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_VIEW_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// The camera live video feed view.
class VideoStreamView : public views::View {
  METADATA_HEADER(VideoStreamView, views::View)

 public:
  explicit VideoStreamView(float default_aspect_ratio);
  VideoStreamView(const VideoStreamView&) = delete;
  VideoStreamView& operator=(const VideoStreamView&) = delete;
  ~VideoStreamView() override;

  void SetRasterContextProvider(
      scoped_refptr<viz::RasterContextProvider> raster_context_provider) {
    raster_context_provider_ = std::move(raster_context_provider);
  }

  void ScheduleFramePaint(scoped_refptr<media::VideoFrame> frame);
  void ClearFrame();

 protected:
  // views::View overrides
  void OnPaint(gfx::Canvas* canvas) override;
  int GetHeightForWidth(int w) const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  float current_aspect_ratio_;
  bool has_updated_preferred_size_ = false;
  const int rounded_radius_;
  media::PaintCanvasVideoRenderer video_renderer_;
  scoped_refptr<media::VideoFrame> latest_frame_;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_VIEW_H_
