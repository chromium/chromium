// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_ICON_WORKER_H_
#define COMPONENTS_FAVICON_CORE_LARGE_ICON_WORKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace favicon {

class FaviconService;

// Processes the png data returned from the FaviconService as part of a
// LargeIconService request (resizing and decoding from PNG format).
class LargeIconWorker : public base::RefCountedThreadSafe<LargeIconWorker> {
 public:
  // Exactly one of the callbacks is expected to be non-null.
  LargeIconWorker(
      int min_source_size_in_pixel,
      int size_in_pixel_to_resize_to,
      LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior,
      favicon_base::LargeIconCallback raw_bitmap_callback,
      favicon_base::LargeIconImageCallback image_callback,
      base::CancelableTaskTracker* tracker);
  LargeIconWorker(const LargeIconWorker& worker) = delete;
  LargeIconWorker& operator=(const LargeIconWorker& worker) = delete;

  // Must run on the owner (UI) thread in production.
  // Intermediate callback for GetLargeIconOrFallbackStyle(). Invokes
  // ProcessIconOnBackgroundThread() so we do not perform complex image
  // operations on the UI thread.
  void OnIconLookupComplete(
      const favicon_base::FaviconRawBitmapResult& db_result);

  static base::CancelableTaskTracker::TaskId GetLargeIconRawBitmap(
      FaviconService* favicon_service,
      const GURL& page_url,
      int min_source_size_in_pixel,
      int size_in_pixel_to_resize_to,
      LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior,
      favicon_base::LargeIconCallback raw_bitmap_callback,
      favicon_base::LargeIconImageCallback image_callback,
      base::CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<LargeIconWorker>;

  ~LargeIconWorker();

  // Must run on the owner (UI) thread in production.
  // Invoked when ProcessIconOnBackgroundThread() is done.
  void OnIconProcessingComplete();

  int min_source_size_in_pixel_;
  int size_in_pixel_to_resize_to_;
  LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior_;
  favicon_base::LargeIconCallback raw_bitmap_callback_;
  favicon_base::LargeIconImageCallback image_callback_;
  scoped_refptr<base::TaskRunner> background_task_runner_;
  raw_ptr<base::CancelableTaskTracker, DanglingUntriaged> tracker_;

  favicon_base::FaviconRawBitmapResult raw_bitmap_result_;
  SkBitmap bitmap_result_;
  GURL icon_url_;
  std::unique_ptr<favicon_base::FallbackIconStyle> fallback_icon_style_;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_ICON_WORKER_H_
