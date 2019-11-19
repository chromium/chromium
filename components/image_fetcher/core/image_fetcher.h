// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace image_fetcher {

class ImageDecoder;
class ReducedModeImageFetcher;

// Encapsulates image fetching customization options.
// (required)
// traffic_annotation
//   Documents what the network traffic is for, gives you free metrics.
// max_download_size
//   Limits the size of the downloaded image.
// frame_size
//   If multiple sizes of the image are available on the server, choose the one
//   that's closest to the given size (only useful for .icos). Does NOT resize
//   the downloaded image to the given dimensions.
class ImageFetcherParams {
  // Allows the bridge to access the private function set_skip_transcoding
  // used for gif download.
  friend class ImageFetcherBridge;
  // Allows ReducedModeImageFetcher to access the private
  // function set_skip_transcoding and set_allow_needs_transcoding_file because
  // it ignores the ImageFetcherCallback.
  friend class ReducedModeImageFetcher;

 public:
  // Sets the UMA client name to report feature-specific metrics. Make sure
  // |uma_client_name| is also present in histograms.xml.
  ImageFetcherParams(
      net::NetworkTrafficAnnotationTag network_traffic_annotation_tag,
      std::string uma_client_name);
  ImageFetcherParams(const ImageFetcherParams& params);
  ImageFetcherParams(ImageFetcherParams&& params);

  ~ImageFetcherParams();

  const net::NetworkTrafficAnnotationTag traffic_annotation() const {
    return network_traffic_annotation_tag_;
  }

  void set_max_download_size(base::Optional<int64_t> max_download_bytes) {
    max_download_bytes_ = max_download_bytes;
  }

  base::Optional<int64_t> max_download_size() const {
    return max_download_bytes_;
  }

  void set_frame_size(gfx::Size desired_frame_size) {
    desired_frame_size_ = desired_frame_size;
  }

  gfx::Size frame_size() const { return desired_frame_size_; }

  const std::string& uma_client_name() const { return uma_client_name_; }

  bool skip_transcoding() const { return skip_transcoding_; }

  bool allow_needs_transcoding_file() const {
    return allow_needs_transcoding_file_;
  }

  // Only to be used in unittests.
  void set_skip_transcoding_for_testing(bool skip_transcoding) {
    skip_transcoding_ = skip_transcoding;
  }

  bool skip_disk_cache_read() { return skip_disk_cache_read_; }

  void set_skip_disk_cache_read(bool skip_disk_cache_read) {
    skip_disk_cache_read_ = skip_disk_cache_read;
  }

 private:
  void set_skip_transcoding(bool skip_transcoding) {
    skip_transcoding_ = skip_transcoding;
  }

  void set_allow_needs_transcoding_file(bool allow_needs_transcoding_file) {
    allow_needs_transcoding_file_ = allow_needs_transcoding_file;
  }

  const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag_;

  base::Optional<int64_t> max_download_bytes_;
  gfx::Size desired_frame_size_;
  std::string uma_client_name_;
  // When true, the image fetcher will skip transcoding whenever possible. Only
  // use this if you've considered the security implications. For instance, in
  // some java clients we decode GIFs entirely in Java which is safe to do
  // in-process without transcoding.
  bool skip_transcoding_;
  // True if the disk cache should be skipped because it was already checked in
  // java.
  bool skip_disk_cache_read_;
  // True if allowing images that need transcoding to be stored with a prefix in
  // file names.
  bool allow_needs_transcoding_file_;
};

// A class used to fetch server images. It can be called from any thread and the
// callback will be called on the thread which initiated the fetch.
class ImageFetcher {
 public:
  ImageFetcher() {}
  virtual ~ImageFetcher() {}

  // Fetch an image and optionally decode it. |image_data_callback| is called
  // when the image fetch completes, but |image_data_callback| may be empty.
  // |image_callback| is called when the image is finished decoding.
  // |image_callback| may be empty if image decoding is not required. If a
  // callback is provided, it will be called exactly once. On failure, an empty
  // string/gfx::Image is returned.
  virtual void FetchImageAndData(const GURL& image_url,
                                 ImageDataFetcherCallback image_data_callback,
                                 ImageFetcherCallback image_callback,
                                 ImageFetcherParams params) = 0;

  // Fetch an image and decode it. An empty gfx::Image will be returned to the
  // callback in case the image could not be fetched. This is the same as
  // calling FetchImageAndData without an |image_data_callback|.
  void FetchImage(const GURL& image_url,
                  ImageFetcherCallback callback,
                  ImageFetcherParams params) {
    FetchImageAndData(image_url, ImageDataFetcherCallback(),
                      std::move(callback), params);
  }

  // Just fetch the image data, do not decode. This is the same as
  // calling FetchImageAndData without an |image_callback|.
  void FetchImageData(const GURL& image_url,
                      ImageDataFetcherCallback callback,
                      ImageFetcherParams params) {
    FetchImageAndData(image_url, std::move(callback), ImageFetcherCallback(),
                      params);
  }

  virtual ImageDecoder* GetImageDecoder() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_H_
