// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/background_thumbnail_video_capturer.h"
#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"
#include "chrome/browser/ui/thumbnails/thumbnail_capture_info.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ThumbnailScheduler;

namespace tabs {
class TabInterface;
}

// Maintains the thumbnail image shown in e.g. tab hover cards. Owned by the
// tab's TabFeatures; only created when a feature that needs thumbnails is
// enabled.
class ThumbnailTabHelper : public content::WebContentsObserver,
                           public ThumbnailCaptureDriver::Client,
                           public ThumbnailImage::Delegate {
 public:
  DECLARE_USER_DATA(ThumbnailTabHelper);

  // `contents` is passed explicitly because during a discard the helper is
  // recreated for the incoming WebContents before `tab` swaps its contents.
  ThumbnailTabHelper(tabs::TabInterface& tab, content::WebContents* contents);

  ThumbnailTabHelper(const ThumbnailTabHelper&) = delete;
  ThumbnailTabHelper& operator=(const ThumbnailTabHelper&) = delete;

  ~ThumbnailTabHelper() override;

  static ThumbnailTabHelper* From(tabs::TabInterface* tab);

  scoped_refptr<ThumbnailImage> thumbnail() const { return thumbnail_; }

  // Notify the helper that the tab is being hidden by being put into the
  // background. Allows for an updated preview image after swapping away from an
  // active tab.
  void CaptureThumbnailOnTabBackgrounded();

 private:
  using CaptureReadiness = ThumbnailImage::CaptureReadiness;

  enum class CaptureType;

  static ThumbnailScheduler& GetScheduler();

  // ThumbnailCaptureDriver::Client:
  void RequestCapture() override;
  void StartCapture() override;
  void StopCapture() override;

  // content::WebContentsObserver:
  void RenderViewReady() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void AboutToBeDiscarded(content::WebContents* new_contents) override;

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override;
  CaptureReadiness GetCaptureReadiness() const override;

  void PageReadinessChanged(CaptureReadiness readiness);

  // Returns the host view associated with the current web contents, or null if
  // none.
  content::RenderWidgetHostView* GetView();

  // Begins periodic capture of thumbnails from a loading page.
  // This can be triggered by someone starting to observe a web contents by
  // incrementing its capture count, or it can happen opportunistically when a
  // renderer is available, because we want to capture thumbnails while we can
  // before a page is frozen or swapped out.
  void StartVideoCapture();
  void StopVideoCapture();

  void StoreThumbnailForTabSwitch(base::TimeTicks start_time,
                                  const content::CopyFromSurfaceResult& result);
  void StoreThumbnailForBackgroundCapture(const SkBitmap& bitmap,
                                          uint64_t frame_id);
  void StoreThumbnail(CaptureType type,
                      const SkBitmap& bitmap,
                      std::optional<uint64_t> frame_id);

  // Clears the data associated to the currently set thumbnail. For when the
  // thumbnail is no longer valid.
  void ClearData();

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

  // Times for computing metrics.
  base::TimeTicks start_video_capture_time_;

  BackgroundThumbnailVideoCapturer background_capturer_;

  // Scoped request for video capture. Declared before `capture_driver_` because
  // `~ThumbnailCaptureDriver()` may call back into `StopCapture()`.
  base::ScopedClosureRunner scoped_capture_;

  ThumbnailCaptureDriver capture_driver_{this, &GetScheduler()};
  ThumbnailReadinessTracker readiness_tracker_;

  // Where we are in the page lifecycle.
  CaptureReadiness page_readiness_ = CaptureReadiness::kNotReady;

  // The thumbnail maintained by this instance.
  scoped_refptr<ThumbnailImage> thumbnail_;

  ui::ScopedUnownedUserData<ThumbnailTabHelper> scoped_unowned_user_data_;

  base::WeakPtrFactory<ThumbnailTabHelper>
      weak_factory_for_thumbnail_on_tab_hidden_{this};
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
