// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BUBBLE_SHOW_OPTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BUBBLE_SHOW_OPTIONS_H_

#include <string>

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "ui/gfx/image/image.h"

namespace autofill {

// Contains the necessary information to pop up the virtual card manual fallback
// bubble.
struct VirtualCardManualFallbackBubbleOptions {
  VirtualCardManualFallbackBubbleOptions();
  VirtualCardManualFallbackBubbleOptions(
      const VirtualCardManualFallbackBubbleOptions&);
  VirtualCardManualFallbackBubbleOptions& operator=(
      const VirtualCardManualFallbackBubbleOptions&);
  ~VirtualCardManualFallbackBubbleOptions();

  bool IsValid() const;

  // The descriptive name of the masked server card that the `virtual_card` is
  // tied to.
  std::u16string masked_card_name;

  // The last four digits of the card number of the masked server card that the
  // `virtual_card` is tied to. Note that this string also contains the leading
  // unicode ellipsis dots, so an example value is "•••• 1234".
  std::u16string masked_card_number_last_four;

  // The credit card object containing information (other than CVC) for the
  // virtual card.
  CreditCard virtual_card;

  // The CVC for the `virtual_card`.
  std::u16string virtual_card_cvc;

  // The card image to be shown in the bubble.
  gfx::Image card_image;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BUBBLE_SHOW_OPTIONS_H_
