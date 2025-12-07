// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TEST_AUTOFILL_IMAGE_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TEST_AUTOFILL_IMAGE_FETCHER_H_

#include <map>
#include <memory>

#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"

class GURL;

namespace gfx {
class Image;
}  // namespace gfx

namespace autofill {

// A simplistic `AutofillImageFetcher` used for testing.
class TestAutofillImageFetcher : public AutofillImageFetcherBase {
 public:
  TestAutofillImageFetcher();
  TestAutofillImageFetcher(const TestAutofillImageFetcher&) = delete;
  TestAutofillImageFetcher(TestAutofillImageFetcher&&) = delete;
  TestAutofillImageFetcher& operator=(const TestAutofillImageFetcher&) = delete;
  TestAutofillImageFetcher& operator=(TestAutofillImageFetcher&&) = delete;

  ~TestAutofillImageFetcher() override;

  // AutofillImageFetcherBase:
  void FetchCreditCardArtImagesForURLs(
      base::span<const GURL> image_urls,
      base::span<const ImageSize> image_sizes) override;
  void FetchPixAccountImagesForURLs(base::span<const GURL> image_urls) override;
  void FetchValuableImagesForURLs(base::span<const GURL> image_urls) override;
  const gfx::Image* GetCachedImageForUrl(const GURL& image_url,
                                         ImageType image_type) const override;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaImageFetcher()
      override;
#endif

  // Adds a `url` to `image` mapping to the local `cached_images_`
  // cache.
  void CacheImage(const GURL& url, const gfx::Image& image);

  void ClearCachedImages() { cached_images_.clear(); }

 private:
  std::map<GURL, std::unique_ptr<gfx::Image>> cached_images_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TEST_AUTOFILL_IMAGE_FETCHER_H_
