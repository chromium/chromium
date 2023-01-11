// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/image_prefetcher.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/query_tiles/internal/image_loader.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/internal/tile_iterator.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace query_tiles {
namespace {

int ToTileIteratorLevel(ImagePrefetchMode mode) {
  switch (mode) {
    case ImagePrefetchMode::kTopLevel:
      return 0;
    case ImagePrefetchMode::kAll:
      return TileIterator::kAllTiles;
    default:
      NOTREACHED();
      return 0;
  }
}

class ImagePrefetcherImpl : public ImagePrefetcher {
 public:
  ImagePrefetcherImpl(ImagePrefetchMode mode,
                      std::unique_ptr<ImageLoader> image_loader)
      : mode_(mode), image_loader_(std::move(image_loader)) {}
  ~ImagePrefetcherImpl() override = default;

 private:
  // ImagePrefetcher implementation.
  void Prefetch(TileGroup tile_group,
                bool is_from_reduced_mode,
                base::OnceClosure done_callback) override {
    DCHECK(done_callback);
    if (mode_ == ImagePrefetchMode::kNone) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(done_callback));
      return;
    }

    // Get the URLs to fetch.
    std::vector<GURL> urls_to_fetch;
    int level = ToTileIteratorLevel(mode_);
    TileIterator it(tile_group, level);
    while (it.HasNext()) {
      const auto* tile = it.Next();
      DCHECK(tile);
      if (tile->image_metadatas.empty())
        continue;
      GURL url = tile->image_metadatas.front().url;
      if (!url.is_valid() || url.is_empty())
        continue;
      urls_to_fetch.emplace_back(std::move(url));
    }

    FetchImages(std::move(urls_to_fetch), is_from_reduced_mode,
                std::move(done_callback));
  }

  // Fetch all images in |urls_to_fetch|.
  void FetchImages(std::vector<GURL> urls_to_fetch,
                   bool is_from_reduced_mode,
                   base::OnceClosure done_callback) {
    // All image urls are fetched.
    if (urls_to_fetch.empty()) {
      DCHECK(done_callback);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(done_callback));
      return;
    }

    // Prefetch images one by one.
    GURL url = urls_to_fetch.back();
    DCHECK(url.is_valid());
    urls_to_fetch.pop_back();
    auto next =
        base::BindOnce(&ImagePrefetcherImpl::FetchImages,
                       weak_ptr_factory_.GetWeakPtr(), std::move(urls_to_fetch),
                       is_from_reduced_mode, std::move(done_callback));
    if (is_from_reduced_mode) {
      // The image won't be decoded in reduced mode.
      image_loader_->PrefetchImage(
          url, base::BindOnce(&ImagePrefetcherImpl::OnImageFetchedInReducedMode,
                              weak_ptr_factory_.GetWeakPtr(), std::move(next)));
    } else {
      image_loader_->FetchImage(
          url, base::BindOnce(&ImagePrefetcherImpl::OnImageFetched,
                              weak_ptr_factory_.GetWeakPtr(), std::move(next)));
    }
  }

  void OnImageFetched(base::OnceClosure next, SkBitmap bitmap) {
    if (next) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(next));
    }
  }

  void OnImageFetchedInReducedMode(base::OnceClosure next, bool success) {
    if (next) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(next));
    }
  }

  ImagePrefetchMode mode_;
  std::unique_ptr<ImageLoader> image_loader_;
  base::WeakPtrFactory<ImagePrefetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<ImagePrefetcher> ImagePrefetcher::Create(
    ImagePrefetchMode prefetch_mode,
    std::unique_ptr<ImageLoader> image_loader) {
  return std::make_unique<ImagePrefetcherImpl>(prefetch_mode,
                                               std::move(image_loader));
}

}  // namespace query_tiles
