// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_

#include "base/functional/callback.h"
#include "url/gurl.h"

class SkBitmap;

namespace query_tiles {

// Loads image for query tiles.
class ImageLoader {
 public:
  using BitmapCallback = base::OnceCallback<void(SkBitmap bitmap)>;
  using SuccessCallback = base::OnceCallback<void(bool)>;

  ImageLoader() = default;
  virtual ~ImageLoader() = default;
  ImageLoader(const ImageLoader&) = delete;
  ImageLoader& operator=(const ImageLoader&) = delete;

  // Fetches the bitmap of an image based on its URL. Callback will be invoked
  // with the bitmap of the image, or an empty bitmap on failure.
  virtual void FetchImage(const GURL& url, BitmapCallback callback) = 0;

  // Prefetches the image data. The decoding will be deferred to next full
  // browser mode launch. Must be called in reduced mode. The |callback| will be
  // invoked after network fetch is done.
  virtual void PrefetchImage(const GURL& url, SuccessCallback callback) = 0;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_
