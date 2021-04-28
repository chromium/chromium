// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card_art_image.h"

namespace autofill {

CreditCardArtImage::CreditCardArtImage() = default;

CreditCardArtImage::CreditCardArtImage(const std::string& id,
                                       int64_t instrument_id,
                                       std::vector<uint8_t> card_art_image) {
  this->id = id;
  this->instrument_id = instrument_id;
  this->card_art_image = std::move(card_art_image);
}

CreditCardArtImage::~CreditCardArtImage() = default;

}  // namespace autofill
