// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_BASE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"

class GURL;

namespace autofill {

struct CreditCardArtImage;

// Abstract class that enables pre-fetching of credit card art images on browser
// start-up.
//
// Subclasses provide interface to use the image fetcher (as this differs per
// platform) and can specialize handling the returned card art image.
//
// On Desktop and iOS:
// 1. AutofillImageFetcher (in components/) extends this class, and implements
// the logic to fetch images.
// 2. AutofillImageFetcherImpl (in chrome/) extends AutofillImageFetcher, and is
// responsible for initializing and exposing the image_fetcher::ImageFetcher to
// the components/ directory.
//
// On Android:
// AutofillImageFetcherImpl (in chrome/) extends this class, and is responsible
// for initializing and storing a reference to the Java AutofillImageFetcher.
// Since fetching, treating, and caching of images is done on the Java layer,
// Android does not need an intermediate class.
class AutofillImageFetcherBase {
 public:
  //  TODO (crbug.com/1478931): The implementation classes should own the
  //  fetched images, and define the callback to handle the images.
  //
  // Once invoked, the image fetcher starts fetching images asynchronously based
  // on the urls. `card_art_urls` is a span of with credit cards' card art image
  // url. `callback` will be invoked when all the requests have been completed.
  // The callback will receive a vector of CreditCardArtImage, for (only) those
  // cards for which the AutofillImageFetcher could successfully fetch the
  // image.
  virtual void FetchImagesForURLs(
      base::span<const GURL> card_art_urls,
      base::OnceCallback<
          void(const std::vector<std::unique_ptr<CreditCardArtImage>>&)>
          callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_BASE_H_
