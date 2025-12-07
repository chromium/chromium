// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_BASE_H_

#include "base/containers/span.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;

namespace gfx {
class Image;
}  // namespace gfx

namespace autofill {

// Abstract class that enables pre-fetching of images from server on browser
// start-up used by various Autofill features.
//
// Subclasses provide interface to use the image fetcher (as this differs per
// platform) and can specialize handling the returned image.
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
  // The types of images supported by the `AutofillImageFetcher` API.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
  enum class ImageType {
    // Art image icons used for credit card display in the payments TTF bottom
    // sheet.
    kCreditCardArtImage,
    // Bank account icons used in the financial account management UI.
    kPixAccountImage,
    // Icons for Google Wallet items that can't be used for payments (like
    // loyalty cards, etc.).
    kValuableImage,
  };

  // Different sizes in which we show the credit card / bank account art images.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
  enum class ImageSize {
    kSmall = 0,
    kLarge = 1,
    kSquare = 2,
  };

  virtual ~AutofillImageFetcherBase() = default;

  // Once invoked, the image fetcher starts fetching images asynchronously based
  // on the urls. `image_urls` is a span of urls that needs to be downloaded. If
  // an image has already been fetched, it won't be fetched again. `image_sizes`
  // is the different sizes in which each image_url should be downloaded. This
  // method is intended for credit card art images because of the image
  // post-processing applied to the downloaded icons.
  virtual void FetchCreditCardArtImagesForURLs(
      base::span<const GURL> image_urls,
      base::span<const ImageSize> image_sizes) = 0;

  // Fetches images for the `image_urls`, treats them according to Pix image
  // specifications, and caches them in memory.
  virtual void FetchPixAccountImagesForURLs(
      base::span<const GURL> image_urls) = 0;

  // Once invoked, the image fetcher starts fetching images asynchronously based
  // on the urls. `image_urls` is a span of urls that needs to be downloaded. If
  // an image has already been fetched, it won't be fetched again. This method
  // is intended for valuable images because of the image post-processing
  // applied to the downloaded icons.
  virtual void FetchValuableImagesForURLs(
      base::span<const GURL> image_urls) = 0;

  // Returns the cached image for the `image_url` if it was fetched locally to
  // the client. If the image is not present in the cache, this function will
  // return a `nullptr`.
  virtual const gfx::Image* GetCachedImageForUrl(
      const GURL& image_url,
      ImageType image_type) const = 0;

#if BUILDFLAG(IS_ANDROID)
  // Return the owned AutofillImageFetcher Java object. It is created if it
  // doesn't already exist.
  virtual base::android::ScopedJavaLocalRef<jobject>
  GetOrCreateJavaImageFetcher() = 0;
#endif
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_BASE_H_
