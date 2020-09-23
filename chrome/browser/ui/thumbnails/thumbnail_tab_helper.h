// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class ThumbnailScheduler;

class ThumbnailTabHelper
    : public content::WebContentsUserData<ThumbnailTabHelper>,
      public viz::mojom::FrameSinkVideoConsumer {
 public:
  ~ThumbnailTabHelper() override;

  scoped_refptr<ThumbnailImage> thumbnail() const { return thumbnail_; }

 private:
  class TabStateTracker;
  friend class content::WebContentsUserData<ThumbnailTabHelper>;

  // Metrics enums and helper functions:
  enum class CaptureType;
  static void RecordCaptureType(CaptureType type);

  // Describes how a thumbnail bitmap should be generated from a target surface.
  // All sizes are in pixels, not DIPs.
  struct ThumbnailCaptureInfo {
    // The total source size (including scrollbars).
    gfx::Size source_size;

    // Insets for scrollbars in the source image that should probably be
    // ignored for thumbnailing purposes.
    gfx::Insets scrollbar_insets;

    // Cropping rectangle for the source canvas, in pixels.
    gfx::Rect copy_rect;

    // Size of the target bitmap in pixels.
    gfx::Size target_size;
  };

  explicit ThumbnailTabHelper(content::WebContents* contents);

  static ThumbnailScheduler& GetScheduler();

  // Begins periodic capture of thumbnails from a loading page.
  // This can be triggered by someone starting to observe a web contents by
  // incrementing its capture count, or it can happen opportunistically when a
  // renderer is available, because we want to capture thumbnails while we can
  // before a page is frozen or swapped out.
  void StartVideoCapture();
  void StopVideoCapture();
  void CaptureThumbnailOnTabHidden();
  void StoreThumbnailForTabSwitch(base::TimeTicks start_time,
                                  const SkBitmap& bitmap);
  void StoreThumbnail(CaptureType type, const SkBitmap& bitmap);

  // viz::mojom::FrameSinkVideoConsumer:
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnStopped() override;
  void OnLog(const std::string& /*message*/) override {}

  // Returns the dimensions of the multipurpose thumbnail that should be
  // captured from an entire webpage. Can be cropped or compressed later.
  // If |include_scrollbars_in_capture| is false, the area which is likely to
  // contain scrollbars will be removed from both the result's |copy_rect| and
  // |target_size|. In both cases, |scrollbar_insets| is calculated. This
  // function always returns a result with |clip_result| = kSourceNotClipped.
  static ThumbnailCaptureInfo GetInitialCaptureInfo(
      const gfx::Size& source_size,
      float scale_factor,
      bool include_scrollbars_in_capture);

  // Copy info from the most recent frame we have captured.
  ThumbnailCaptureInfo last_frame_capture_info_;

  // Captures frames from the WebContents while it's hidden. The capturer count
  // of the WebContents is incremented/decremented when a capturer is set/unset.
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  // Private implementation of state tracking.
  std::unique_ptr<TabStateTracker> state_;

  // Times for computing metrics.
  base::TimeTicks start_video_capture_time_;

  // Whether the first frame has been received after StartVideoCapture().
  bool got_first_frame_ = false;

  // The thumbnail maintained by this instance.
  scoped_refptr<ThumbnailImage> thumbnail_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ThumbnailTabHelper>
      weak_factory_for_thumbnail_on_tab_hidden_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelper);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
