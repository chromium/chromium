// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_THUMBNAIL_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_THUMBNAIL_OBSERVER_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

// Tracks a specific thumbnail, or no thumbnail. Provides a callback for when
// image data for the current thumbnail is available.
class TabHoverCardThumbnailObserver {
 public:
  using CallbackSignature = void(TabHoverCardThumbnailObserver* observer,
                                 gfx::ImageSkia thumbnail);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  TabHoverCardThumbnailObserver();
  ~TabHoverCardThumbnailObserver();

  // Begin watching the specified thumbnail image for updates. Ideally, should
  // trigger the associated WebContents to load (if not loaded already) and
  // retrieve a valid thumbnail.
  void Observe(scoped_refptr<ThumbnailImage> thumbnail_image);

  // Returns the current (most recent) thumbnail being watched.
  const scoped_refptr<ThumbnailImage>& current_image() const {
    return current_image_;
  }

  base::CallbackListSubscription AddCallback(Callback callback);

 private:
  void ThumbnailImageCallback(const ThumbnailImage* image,
                              gfx::ImageSkia preview_image);

  scoped_refptr<ThumbnailImage> current_image_;
  std::unique_ptr<ThumbnailImage::Subscription> subscription_;
  base::RepeatingCallbackList<CallbackSignature> callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_THUMBNAIL_OBSERVER_H_
