// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_BACKGROUND_THUMBNAIL_VIDEO_CAPTURER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_BACKGROUND_THUMBNAIL_VIDEO_CAPTURER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/background_thumbnail_capturer.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}  // namespace content

// A thumbnail capturer using viz::ClientFrameSinkVideoCapturer. Gets a
// sequence of frames in the same way as streaming a tab.
class BackgroundThumbnailVideoCapturer
    : public BackgroundThumbnailCapturer,
      public viz::mojom::FrameSinkVideoConsumer {
 public:
  // Client receives `SkBitmap` frames and `uin64_t` unique IDs for each
  // frame. IDs are globally unique for a given browser process and are
  // used for TRACE_EVENT_FLOW_* macros
  using GotFrameCallback =
      base::RepeatingCallback<void(const SkBitmap&, uint64_t)>;
  BackgroundThumbnailVideoCapturer(content::WebContents* contents,
                                   GotFrameCallback got_frame_callback);
  ~BackgroundThumbnailVideoCapturer() override;

  // BackgroundThumbnailCapturer:
  void Start(const ThumbnailCaptureInfo& capture_info) override;
  void Stop() override;

 private:
  // viz::mojom::FrameSinkVideoConsumer:
  void OnFrameCaptured(
      ::media::mojom::VideoBufferHandlePtr data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnStopped() override;
  void OnLog(const std::string& /*message*/) override;

  const raw_ptr<content::WebContents> contents_;
  GotFrameCallback got_frame_callback_;

  ThumbnailCaptureInfo capture_info_;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  // Tracked for metrics and tracing
  base::TimeTicks start_time_;
  int num_received_frames_ = 0;
  uint64_t cur_capture_num_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_BACKGROUND_THUMBNAIL_VIDEO_CAPTURER_H_
