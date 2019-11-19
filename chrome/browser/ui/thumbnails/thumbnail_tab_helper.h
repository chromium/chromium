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
#include "chrome/browser/ui/thumbnails/thumbnail_page_event_adapter.h"
#include "chrome/browser/ui/thumbnails/thumbnail_page_observer.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class ThumbnailTabHelper
    : public content::WebContentsUserData<ThumbnailTabHelper>,
      public content::WebContentsObserver,
      public viz::mojom::FrameSinkVideoConsumer,
      public ThumbnailImage::Delegate {
 public:
  ~ThumbnailTabHelper() override;

  scoped_refptr<ThumbnailImage> thumbnail() const { return thumbnail_; }

 private:
  class ThumanailImageImpl;
  class ScopedCapture;
  friend class content::WebContentsUserData<ThumbnailTabHelper>;
  friend class ThumanailImageImpl;

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

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override;

  bool ShouldKeepUpdatingThumbnail() const;

  void StartVideoCapture();
  void StopVideoCapture();
  void CaptureThumbnailOnTabSwitch();
  void StoreThumbnail(const SkBitmap& bitmap);

  content::RenderWidgetHostView* GetView();

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderViewReady() override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;

  // viz::mojom::FrameSinkVideoConsumer:
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnStopped() override;

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

  // The implementation of the 'classic' thumbnail cropping algorithm. It is not
  // content-driven in any meaningful way. Rather, the choice of a cropping
  // region is based on relation between source and target sizes. The selected
  // source region is then rescaled into the target thumbnail image.
  //
  // Provides information necessary to crop-and-resize image data from a source
  // canvas of |source_size|. Auxiliary |scale_factor| helps compute the target
  // thumbnail size to be copied from the backing store, in pixels. The return
  // value contains the type of clip and the clip parameters.
  // static ThumbnailCaptureInfo GetThumbnailCropInfo(const gfx::Size&
  // source_size,
  //  float scale_factor,
  //  const gfx::Size& unscaled_target_size);

  // The last known visibility WebContents visibility.
  content::Visibility last_visibility_;

  // Is the thumbnail being observed?
  bool is_being_observed_ = false;

  // Whether a thumbnail was captured while the tab was loaded, since the tab
  // was last hidden.
  bool captured_loaded_thumbnail_since_tab_hidden_ = false;

  // Copy info from the most recent frame we have captured.
  ThumbnailCaptureInfo last_frame_capture_info_;

  // Captures frames from the WebContents while it's hidden. The capturer count
  // of the WebContents is incremented/decremented when a capturer is set/unset.
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  // Scoped request for video capture. Ensures we always decrement the counter
  // once per increment.
  std::unique_ptr<ScopedCapture> scoped_capture_;

  // The thumbnail maintained by this instance.
  scoped_refptr<ThumbnailImage> thumbnail_ =
      base::MakeRefCounted<ThumbnailImage>(this);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ThumbnailTabHelper>
      weak_factory_for_thumbnail_on_tab_hidden_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelper);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
