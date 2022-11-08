// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"

namespace autofill {

VirtualCardManualFallbackBubbleOptions::
    VirtualCardManualFallbackBubbleOptions() = default;
VirtualCardManualFallbackBubbleOptions::VirtualCardManualFallbackBubbleOptions(
    const VirtualCardManualFallbackBubbleOptions&) = default;
VirtualCardManualFallbackBubbleOptions&
VirtualCardManualFallbackBubbleOptions::operator=(
    const VirtualCardManualFallbackBubbleOptions&) = default;
VirtualCardManualFallbackBubbleOptions::
    ~VirtualCardManualFallbackBubbleOptions() = default;

bool VirtualCardManualFallbackBubbleOptions::IsValid() const {
  return !masked_card_name.empty() && !masked_card_number_last_four.empty() &&
         virtual_card.HasValidCardNumber() && virtual_card.HasNameOnCard() &&
         virtual_card.HasValidExpirationDate() && !virtual_card_cvc.empty() &&
         !card_image.IsEmpty();
}

}  // namespace autofill
