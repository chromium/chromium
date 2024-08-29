// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/thumbnail_capture_info.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class BackgroundThumbnailCapturer;
class ThumbnailScheduler;

class ThumbnailTabHelper
    : public content::WebContentsUserData<ThumbnailTabHelper>,
      public content::WebContentsObserver {
 public:
  ThumbnailTabHelper(const ThumbnailTabHelper&) = delete;
  ThumbnailTabHelper& operator=(const ThumbnailTabHelper&) = delete;

  ~ThumbnailTabHelper() override;

  scoped_refptr<ThumbnailImage> thumbnail() const { return thumbnail_; }

  bool is_tab_discarded() const { return is_tab_discarded_; }

  // Notify the helper that the tab is being hidden by being put into the
  // background. Allows for an updated preview image after swapping away from an
  // active tab.
  void CaptureThumbnailOnTabBackgrounded();

 private:
  class TabStateTracker;
  friend class content::WebContentsUserData<ThumbnailTabHelper>;

  // Metrics enums and helper functions:
  enum class CaptureType;

  explicit ThumbnailTabHelper(content::WebContents* contents);

  static ThumbnailScheduler& GetScheduler();

  // Begins periodic capture of thumbnails from a loading page.
  // This can be triggered by someone starting to observe a web contents by
  // incrementing its capture count, or it can happen opportunistically when a
  // renderer is available, because we want to capture thumbnails while we can
  // before a page is frozen or swapped out.
  void StartVideoCapture();
  void StopVideoCapture();

  void StoreThumbnailForTabSwitch(base::TimeTicks start_time,
                                  const SkBitmap& bitmap);
  void StoreThumbnailForBackgroundCapture(const SkBitmap& bitmap,
                                          uint64_t frame_id);
  void StoreThumbnail(CaptureType type,
                      const SkBitmap& bitmap,
                      std::optional<uint64_t> frame_id);

  // Clears the data associated to the currently set thumbnail. For when the
  // thumbnail is no longer valid.
  void ClearData();

  // viz::mojom::FrameSinkVideoConsumer:

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

  void AboutToBeDiscarded(content::WebContents* new_contents) override;

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Copy info from the most recent frame we have captured.
  ThumbnailCaptureInfo last_frame_capture_info_;

  // Private implementation of state tracking.
  std::unique_ptr<TabStateTracker> state_;

  std::unique_ptr<BackgroundThumbnailCapturer> background_capturer_;

  // Times for computing metrics.
  base::TimeTicks start_video_capture_time_;

  // Whether the first frame has been received after StartVideoCapture().
  bool got_first_frame_ = false;

  // The thumbnail maintained by this instance.
  scoped_refptr<ThumbnailImage> thumbnail_;

  bool is_tab_discarded_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ThumbnailTabHelper>
      weak_factory_for_thumbnail_on_tab_hidden_{this};
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
