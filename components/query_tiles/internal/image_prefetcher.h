// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_IMAGE_PREFETCHER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_IMAGE_PREFETCHER_H_

#include <memory>

#include "base/callback.h"
#include "components/query_tiles/internal/tile_types.h"

namespace query_tiles {

class ImageLoader;
struct TileGroup;

// Used to prefetch images for a tile group in a background task.
class ImagePrefetcher {
 public:
  static std::unique_ptr<ImagePrefetcher> Create(
      ImagePrefetchMode prefetch_mode,
      std::unique_ptr<ImageLoader> image_loader);

  ImagePrefetcher() = default;
  virtual ~ImagePrefetcher() = default;

  ImagePrefetcher(const ImagePrefetcher&) = delete;
  ImagePrefetcher& operator=(const ImagePrefetcher&) = delete;

  // Prefetch a |tile_group|. |done_callback| will be invoked after prefetching
  // is done.
  virtual void Prefetch(TileGroup tile_group,
                        bool is_from_reduced_mode,
                        base::OnceClosure done_callback) = 0;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_IMAGE_PREFETCHER_H_
