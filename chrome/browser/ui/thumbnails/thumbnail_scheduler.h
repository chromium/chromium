// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_SCHEDULER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_SCHEDULER_H_

#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"
#include "chrome/browser/ui/thumbnails/thumbnail_readiness_tracker.h"

class ThumbnailScheduler {
 public:
  using PageReadiness = ThumbnailReadinessTracker::Readiness;

  class TabCapturer {
   public:
    // Called with true when the scheduler permits a tab to start
    // capturing, or with false when it is no longer allowed to capture.
    // Will only be true for tabs that want to capture. Gets set to false
    // if a tab doesn't want capture anymore.
    //
    // Capture should begin immediately when permitted, and should stop
    // immediately when disallowed.
    virtual void SetCapturePermittedByScheduler(bool capture_permitted) = 0;

   protected:
    virtual ~TabCapturer() = default;
  };

  enum class TabCapturePriority {
    kNone = 0,
    kLow,
    kHigh,
  };

  ThumbnailScheduler() = default;
  virtual ~ThumbnailScheduler() = default;

  virtual void AddTab(TabCapturer* tab) = 0;
  virtual void RemoveTab(TabCapturer* tab) = 0;

  virtual void SetTabCapturePriority(TabCapturer* tab,
                                     TabCapturePriority priority) = 0;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_SCHEDULER_H_
