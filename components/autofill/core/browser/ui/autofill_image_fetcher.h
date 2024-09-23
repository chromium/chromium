// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_

#include <memory>
#include <optional>

#include "base/barrier_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"

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
  virtual ~AutofillImageFetcher() = default;

  // AutofillImageFetcherBase:
  // The image sizes passed in the arguments are unused as this param is only
  // used for Android. For Desktop, the implementation of this method has
  // hardcoded image sizes.
  void FetchImagesForURLs(
      base::span<const GURL> image_urls,
      base::span<const AutofillImageFetcherBase::ImageSize> image_sizes_unused,
      base::OnceCallback<void(
          const std::vector<std::unique_ptr<CreditCardArtImage>>&)> callback)
      override;

  // Subclasses may override this to provide custom handling of a given card art
  // URL.
  virtual GURL ResolveCardArtURL(const GURL& card_art_url);

  // Subclasses may override this to provide custom handling of a fetched card
  // art image. The default behavior is a no-op. The passed-in `card_art_url` is
  // the original URL before resolving via `ResolveCardArtURL`.
  virtual gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                         const gfx::Image& card_art_image);

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
  // Called when an image is fetched. If the fetch was unsuccessful,
  // `card_art_image` will be an empty gfx::Image(). If the original URL was
  // invalid, `fetch_image_request_timestamp` will also be null.
  void OnCardArtImageFetched(
      base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
          barrier_callback,
      const GURL& card_art_url,
      const std::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& card_art_image,
      const image_fetcher::RequestMetadata& metadata);

 private:
  void FetchImageForURL(
      base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
          barrier_callback,
      const GURL& card_art_url);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
