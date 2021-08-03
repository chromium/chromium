// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_ART_IMAGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_ART_IMAGE_H_

#include <string>
#include <vector>

namespace autofill {

// Represents an rich card art image for the server credit card.
struct CreditCardArtImage {
 public:
  CreditCardArtImage();
  CreditCardArtImage(const std::string& id,
                     int64_t instrument_id,
                     std::vector<uint8_t> card_art_image);
  ~CreditCardArtImage();
  CreditCardArtImage(const CreditCardArtImage&) = delete;
  CreditCardArtImage& operator=(const CreditCardArtImage&) = delete;

  // The server id for the related credit card.
  std::string id;

  // The instrument id for the related credit card.
  int64_t instrument_id;

  // The customized card art image. Stored as an raw PNG-encoded data.
  std::vector<uint8_t> card_art_image;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_ART_IMAGE_H_
