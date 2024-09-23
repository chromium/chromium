// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_READINESS_TRACKER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_READINESS_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Determines how ready a page is for thumbnail capture based on
// navigation and loading state.
class ThumbnailReadinessTracker : public content::WebContentsObserver {
 public:
  using Readiness = ThumbnailImage::CaptureReadiness;
  using ReadinessChangeCallback = base::RepeatingCallback<void(Readiness)>;

  // |web_contents| should be a newly-created contents. If not, the
  // output readiness states will not be correct. |callback| will be
  // called with the new ready state whenever it changes.
  ThumbnailReadinessTracker(content::WebContents* web_contents,
                            ReadinessChangeCallback callback);
  ~ThumbnailReadinessTracker() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;
  void WasDiscarded() override;

 private:
  void UpdateReadiness(Readiness readiness);

  ReadinessChangeCallback callback_;
  Readiness last_readiness_ = Readiness::kNotReady;

  // The last navigation that reset the thumbnail. When this navigation
  // finishes, the page is considered ready for capture.
  raw_ptr<content::NavigationHandle> pending_navigation_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_READINESS_TRACKER_H_
