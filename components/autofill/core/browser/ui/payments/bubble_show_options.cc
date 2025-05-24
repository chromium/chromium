// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"

namespace autofill {

FilledCardInformationBubbleOptions::FilledCardInformationBubbleOptions() =
    default;
FilledCardInformationBubbleOptions::FilledCardInformationBubbleOptions(
    const FilledCardInformationBubbleOptions&) = default;
FilledCardInformationBubbleOptions&
FilledCardInformationBubbleOptions::operator=(
    const FilledCardInformationBubbleOptions&) = default;
FilledCardInformationBubbleOptions::~FilledCardInformationBubbleOptions() =
    default;

bool FilledCardInformationBubbleOptions::IsValid() const {
  return !masked_card_name.empty() && !masked_card_number_last_four.empty() &&
         filled_card.HasValidCardNumber() && filled_card.HasNameOnCard() &&
         filled_card.HasValidExpirationDate() && !cvc.empty() &&
         !card_image.IsEmpty();
}

}  // namespace autofill
