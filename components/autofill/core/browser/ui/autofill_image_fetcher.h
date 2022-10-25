// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"

class GURL;

namespace autofill {

struct CreditCardArtImage;

using CardArtImageData = std::vector<std::unique_ptr<CreditCardArtImage>>;

// Interface that exposes image fetcher for images stored outside of Chrome.
class AutofillImageFetcher {
 public:
  virtual ~AutofillImageFetcher() = default;

  // Once invoked, the image fetcher starts fetching images asynchronously based
  // on the urls. |card_art_urls| is a span of with credit cards' card art image
  // url. |callback| will be invoked when all the requests have been completed.
  // The callback will receive a vector of CreditCardArtImage, for (only) those
  // cards for which the AutofillImageFetcher could successfully fetch the
  // image.
  virtual void FetchImagesForURLs(
      base::span<const GURL> card_art_urls,
      base::OnceCallback<void(const CardArtImageData&)> callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_FETCHER_H_
