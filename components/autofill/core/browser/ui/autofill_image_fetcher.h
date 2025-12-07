// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/barrier_callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/image_fetcher/core/image_fetcher_types.h"

class GURL;

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace autofill {

// Abstract class for Desktop and iOS that exposes image fetcher for images
// stored outside of Chrome.
//
// Subclasses provide the underlying platform-specific
// image_fetcher::ImageFetcher.
class AutofillImageFetcher : public AutofillImageFetcherBase {
 public:
  ~AutofillImageFetcher() override;

  // AutofillImageFetcherBase:
  // The image sizes passed in the arguments are unused as this param is only
  // used for Android. For Desktop, the implementation of this method has
  // hardcoded image sizes.
  void FetchCreditCardArtImagesForURLs(
      base::span<const GURL> image_urls,
      base::span<const AutofillImageFetcherBase::ImageSize> image_sizes_unused)
      override;
  void FetchPixAccountImagesForURLs(base::span<const GURL> image_urls) override;
  void FetchValuableImagesForURLs(base::span<const GURL> image_urls) override;
  const gfx::Image* GetCachedImageForUrl(const GURL& image_url,
                                         ImageType image_type) const override;

  // Subclasses may override this to provide custom handling of a given card art
  // URL for `image_type`. Resolved URLs are used as mapping keys for image
  // caching.
  virtual GURL ResolveImageURL(const GURL& image_url,
                               ImageType image_type) const = 0;

  // Applies platform-specific post-processing to the `image` of the given
  // `image_type`. The passed-in `image_url` is the original URL before
  // resolving via `ResolveImageURL`.
  virtual gfx::Image ResolveImage(const GURL& image_url,
                                  const gfx::Image& image,
                                  ImageType image_type);

  // Subclasses override this to provide the underlying image fetcher instance.
  //
  // Non-const because some platforms initialize the image fetcher dynamically
  // as needed.
  virtual image_fetcher::ImageFetcher* GetImageFetcher() = 0;

  // Subclasses override this to provide a weak-pointer to this class. This is
  // not done in the parent class as the base::WeakPtrFactory member must be the
  // last member in the object, which is not possible to ensure for
  // superclasses.
  virtual base::WeakPtr<AutofillImageFetcher> GetWeakPtr() = 0;

 protected:
  AutofillImageFetcher();

  // Called when an image is fetched. If the fetch was unsuccessful,
  // `image` will be an empty gfx::Image().
  void OnImageFetched(const GURL& image_url,
                      ImageType image_type,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& metadata);

  // Subclasses may override this to provide custom handling of a fetched card
  // art image. The passed-in `card_art_url` is the original URL before
  // resolving via `ResolveImageURL`.
  virtual gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                         const gfx::Image& card_art_image) = 0;
  // Subclasses may override this to provide custom handling of a fetched
  // valuable image.
  virtual gfx::Image ResolveValuableImage(const gfx::Image& valuable_image) = 0;

 private:
  void FetchImageForURL(const GURL& image_url, ImageType image_type);

  // Keeps track of the number of fetch attempts for a given URL.
  std::map<GURL, int> fetch_attempt_counter_;
  // An in-memory image cache which stores post-processed images.
  std::map<GURL, std::unique_ptr<gfx::Image>> cached_images_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
