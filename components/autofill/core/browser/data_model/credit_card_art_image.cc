// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card_art_image.h"

namespace autofill {

CreditCardArtImage::CreditCardArtImage(const GURL& card_art_url,
                                       const gfx::Image& card_art_image)
    : card_art_url(card_art_url), card_art_image(card_art_image) {}

CreditCardArtImage::CreditCardArtImage(const CreditCardArtImage& other) =
    default;

CreditCardArtImage::~CreditCardArtImage() = default;

}  // namespace autofill
