// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/test_autofill_image_fetcher.h"

#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

TestAutofillImageFetcher::TestAutofillImageFetcher() = default;
TestAutofillImageFetcher::~TestAutofillImageFetcher() = default;

void TestAutofillImageFetcher::FetchCreditCardArtImagesForURLs(
    base::span<const GURL> image_urls,
    base::span<const ImageSize> image_sizes) {}

void TestAutofillImageFetcher::FetchPixAccountImagesForURLs(
    base::span<const GURL> image_urls) {}

void TestAutofillImageFetcher::FetchValuableImagesForURLs(
    base::span<const GURL> image_urls) {}

const gfx::Image* TestAutofillImageFetcher::GetCachedImageForUrl(
    const GURL& image_url,
    ImageType image_type) const {
  auto it = cached_images_.find(image_url);
  if (it == cached_images_.end()) {
    return nullptr;
  }
  const gfx::Image* const image = it->second.get();
  return !image->IsEmpty() ? image : nullptr;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
TestAutofillImageFetcher::GetOrCreateJavaImageFetcher() {
  return {};
}
#endif

void TestAutofillImageFetcher::CacheImage(const GURL& url,
                                          const gfx::Image& image) {
  cached_images_[url] = std::make_unique<gfx::Image>(image);
}

}  // namespace autofill
