// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageDecoder;
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GURL;

namespace autofill {

struct CreditCardArtImage;

using CardArtImagesFetchedCallback = base::OnceCallback<void(
    std::vector<std::unique_ptr<CreditCardArtImage>> card_art_images)>;

// The image fetching operation instance. It tracks the state of the paired
// request. Will be created when the AutofillImageFetcher receives a request to
// fetch images for a vector of urls.
class ImageFetchOperation : public base::RefCounted<ImageFetchOperation> {
 public:
  ImageFetchOperation(size_t image_count,
                      CardArtImagesFetchedCallback callback);
  ImageFetchOperation(const ImageFetchOperation&) = delete;
  ImageFetchOperation& operator=(const ImageFetchOperation&) = delete;

  // Invoked when an image fetch is complete and data is returned.
  void ImageFetched(
      const GURL& card_art_url,
      const gfx::Image& card_art_image,
      const absl::optional<base::TimeTicks>& fetch_image_request_timestamp);

 private:
  friend class base::RefCounted<ImageFetchOperation>;

  ~ImageFetchOperation();

  // The number of images that should be fetched before completion.
  size_t pending_request_count_ = 0;

  // The vector of the fetched CreditCardArtImages.
  std::vector<std::unique_ptr<CreditCardArtImage>> fetched_card_art_images_;

  // Callback function to be invoked when fetching is finished.
  CardArtImagesFetchedCallback all_fetches_complete_callback_;
};

// Fetches images stored outside of Chrome for autofill.
class AutofillImageFetcher : public KeyedService {
 public:
  AutofillImageFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<image_fetcher::ImageDecoder> image_decoder);
  ~AutofillImageFetcher() override;
  AutofillImageFetcher(const AutofillImageFetcher&) = delete;
  AutofillImageFetcher& operator=(const AutofillImageFetcher&) = delete;

  // Once invoked, the |image_fetcher_| will start fetching images based on the
  // urls. |card_art_urls| is a vector with credit cards' card art image url.
  // |callback| will be invoked when all the requests have been completed. The
  // callback will receive a vector of CreditCardArtImage, for (only) those
  // cards for which the AutofillImageFetcher could successfully fetch the
  // image.
  void FetchImagesForUrls(const std::vector<GURL>& card_art_urls,
                          CardArtImagesFetchedCallback callback);

 protected:
  // Helper function to fetch art image for card given the |card_art_url|, for
  // the specific |operation| instance.
  virtual void FetchImageForUrl(
      const scoped_refptr<ImageFetchOperation>& operation,
      const GURL& card_art_url);

  // The image fetcher implementation.
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

 private:
  friend class AutofillImageFetcherTest;

  // Called when an image is fetched for the |operation| instance.
  static void OnCardArtImageFetched(
      const scoped_refptr<ImageFetchOperation>& operation,
      const GURL& card_art_url,
      const absl::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& card_art_image,
      const image_fetcher::RequestMetadata& metadata);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
