// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_BACKGROUND_THUMBNAIL_CAPTURER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_BACKGROUND_THUMBNAIL_CAPTURER_H_

#include "chrome/browser/ui/thumbnails/thumbnail_capture_info.h"
#include "ui/gfx/geometry/size.h"

// Captures thumbnails from a background tab's contents.
class BackgroundThumbnailCapturer {
 public:
  BackgroundThumbnailCapturer() = default;
  virtual ~BackgroundThumbnailCapturer() = default;

  // Begins capture. The tab's renderer must be alive. The subclass will
  // determine how captured frames are reported to the client.
  //
  // This must be called from the browser UI thread
  virtual void Start(const ThumbnailCaptureInfo& capture_info) = 0;

  // Ends capture. After this call, the tab no longer needs to be kept
  // alive.
  //
  // This must be called from the browser UI thread
  virtual void Stop() = 0;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_BACKGROUND_THUMBNAIL_CAPTURER_H_
