// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_VIEW_H_

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// The camera live video feed view.
class VideoStreamView : public views::View, public viz::ContextLostObserver {
  METADATA_HEADER(VideoStreamView, views::View)

 public:
  VideoStreamView();
  VideoStreamView(const VideoStreamView&) = delete;
  VideoStreamView& operator=(const VideoStreamView&) = delete;
  ~VideoStreamView() override;

  // viz::ContextLostObserver.
  void OnContextLost() override;

  void ScheduleFramePaint(scoped_refptr<media::VideoFrame> frame);
  void ClearFrame();
  size_t GetRenderedFrameCount();

  // views::View overrides
  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  // views::View overrides
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  void OnThemeChanged() override;

 private:
  const float targeted_aspect_ratio_;
  const int rounded_radius_;
  SkColor preview_base_color_;
  media::PaintCanvasVideoRenderer video_renderer_;
  scoped_refptr<media::VideoFrame> latest_frame_;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;
  size_t rendered_frame_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_VIEW_H_
