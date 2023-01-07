// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_THUMBNAIL_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_THUMBNAIL_TRACKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class WebContents;
}

// Tracks the thumbnails of a set of WebContentses. This set is dynamically
// managed, and WebContents closure is handled gracefully. The user is notified
// of any thumbnail change via a callback specified on construction.
class ThumbnailTracker {
 public:
  using CompressedThumbnailData = ThumbnailImage::CompressedThumbnailData;

  // Should return the ThumbnailImage instance for a WebContents.
  using GetThumbnailCallback =
      base::RepeatingCallback<scoped_refptr<ThumbnailImage>(
          content::WebContents*)>;

  // Handles a thumbnail update for a tab.
  using ThumbnailUpdatedCallback =
      base::RepeatingCallback<void(content::WebContents*,
                                   CompressedThumbnailData)>;

  explicit ThumbnailTracker(ThumbnailUpdatedCallback callback);
  // Specifies how to get a ThumbnailImage for a WebContents. This is intended
  // for tests.
  ThumbnailTracker(ThumbnailUpdatedCallback callback,
                   GetThumbnailCallback thumbnail_getter);

  ~ThumbnailTracker();

  // Registers a tab to receive thumbnail updates for. Also immediately requests
  // the current thumbnail.
  void AddTab(content::WebContents* contents);
  void RemoveTab(content::WebContents* contents);

 private:
  void ThumbnailUpdated(content::WebContents* contents,
                        CompressedThumbnailData image);
  void ContentsClosed(content::WebContents* contents);

  // The default |GetThumbnailCallback| implementation.
  static scoped_refptr<ThumbnailImage> GetThumbnailFromTabHelper(
      content::WebContents* contents);

  GetThumbnailCallback thumbnail_getter_;
  ThumbnailUpdatedCallback callback_;

  // ContentsData tracks a particular WebContents. One is created for a tab on
  // its first thumbnail request and exists until the contents is closed.
  class ContentsData;
  base::flat_map<content::WebContents*, std::unique_ptr<ContentsData>>
      contents_data_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_THUMBNAIL_TRACKER_H_
