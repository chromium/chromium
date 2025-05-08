// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_IMAGE_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_IMAGE_FETCHER_H_

#include "base/containers/span.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

// Mock version of AutofillImageFetcherBase.
class MockAutofillImageFetcher : public AutofillImageFetcherBase {
 public:
  MockAutofillImageFetcher();
  ~MockAutofillImageFetcher() override;

  MOCK_METHOD(
      void,
      FetchCreditCardArtImagesForURLs,
      (base::span<const GURL> card_art_urls,
       base::span<const AutofillImageFetcherBase::ImageSize> image_sizes),
      (override));
  MOCK_METHOD(void,
              FetchPixAccountImagesForURLs,
              (base::span<const GURL> card_art_urls),
              (override));
  MOCK_METHOD(void,
              FetchValuableImagesForURLs,
              (base::span<const GURL> image_urls),
              (override));
  MOCK_METHOD(const gfx::Image*,
              GetCachedImageForUrl,
              (const GURL& image_url, ImageType image_type),
              (const, override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetOrCreateJavaImageFetcher,
              (),
              (override));
#endif
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_IMAGE_FETCHER_H_
