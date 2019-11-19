// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace image_fetcher {

class ImageCache;
class ImageFetcher;
struct RequestMetadata;

// Encapsulates a request to simplify argument lists.
struct CachedImageFetcherRequest;

// CachedImageFetcher takes care of fetching images from the network and caching
// them. Has a read-only mode which doesn't perform write operations on the
// cache.
class CachedImageFetcher : public ImageFetcher {
 public:
  CachedImageFetcher(ImageFetcher* image_fetcher,
                     scoped_refptr<ImageCache> image_cache,
                     bool read_only);
  ~CachedImageFetcher() override;

  // ImageFetcher:
  void FetchImageAndData(const GURL& image_url,
                         ImageDataFetcherCallback image_data_callback,
                         ImageFetcherCallback image_callback,
                         ImageFetcherParams params) override;
  ImageDecoder* GetImageDecoder() override;

 private:
  // Cache
  void OnImageFetchedFromCache(CachedImageFetcherRequest request,
                               ImageDataFetcherCallback image_data_callback,
                               ImageFetcherCallback image_callback,
                               bool cache_result_needs_transcoding,
                               std::string image_data);
  void OnImageDecodedFromCache(CachedImageFetcherRequest request,
                               ImageDataFetcherCallback image_data_callback,
                               ImageFetcherCallback image_callback,
                               bool cache_result_needs_transcoding,
                               const gfx::Image& image);

  // Network
  void EnqueueFetchImageFromNetwork(
      CachedImageFetcherRequest request,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback);
  void FetchImageFromNetwork(
      CachedImageFetcherRequest request,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback);
  void OnImageFetchedWithoutTranscoding(
      CachedImageFetcherRequest request,
      ImageDataFetcherCallback image_data_callback,
      const std::string& image_data,
      const RequestMetadata& request_metadata);
  void OnImageFetchedForTranscoding(CachedImageFetcherRequest request,
                                    ImageFetcherCallback image_data_callback,
                                    const gfx::Image& image,
                                    const RequestMetadata& request_metadata);
  // Encode the given |image_data| and store it.
  // |cache_result_needs_transcoding| is passed along for metrics purposes. It
  // is true when the result 1) comes from cache and 2) wasn't transcoded when
  // last time it was stored in the ImageCache.
  // |is_image_data_transcoded| indicates whether the image to save has been
  // transcoded. An image could be transcoded during network fetching, or loaded
  // from the ImageCache without transcoding before.
  void EncodeAndStoreData(bool cache_result_needs_transcoding,
                          bool is_image_data_transcoded,
                          CachedImageFetcherRequest request,
                          const gfx::Image& image);
  // Store the given |image_data| in the cache. |cache_result_needs_transcoding|
  // is passed along for metrics purposes. It is true when the result 1) comes
  // from cache and 2) wasn't transcoded when last time it was stored in the
  // ImageCache.
  void StoreData(bool cache_result_needs_transcoding,
                 bool is_image_data_transcoded,
                 CachedImageFetcherRequest request,
                 std::string image_data);

  // Owned by ImageFetcherService.
  ImageFetcher* image_fetcher_;
  scoped_refptr<ImageCache> image_cache_;

  // Whether the ImageCache is allowed to be modified in any way from requests
  // made by this CachedImageFetcher. This includes updating last used times,
  // writing new data to the cache, or cleaning up unreadable data. Note that
  // the ImageCache may still decide to perform eviction/reconciliation even
  // when only read only CachedImageFetchers are using it.
  bool read_only_;

  // Used to ensure that operations are performed on the sequence that this
  // object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CachedImageFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_
