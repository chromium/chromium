// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_ART_IMAGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_ART_IMAGE_H_

#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

// Represents an rich card art image for the card art url.
struct CreditCardArtImage {
 public:
  CreditCardArtImage(const GURL& card_art_url,
                     const gfx::Image& card_art_image);
  CreditCardArtImage(const CreditCardArtImage& other);
  ~CreditCardArtImage();

  // The url to fetch the card art image.
  GURL card_art_url;

  // The customized card art image.
  gfx::Image card_art_image;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_ART_IMAGE_H_
