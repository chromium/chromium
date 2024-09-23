// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_BUBBLE_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_BUBBLE_BASE_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"

namespace autofill {
class AddressBubbleBaseView : public AutofillLocationBarBubble {
  using AutofillLocationBarBubble::AutofillLocationBarBubble;

  // TODO(crbug.com/325440757): Add common for Save/UpdateAddressProfileView
  // functions.
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_BUBBLE_BASE_VIEW_H_
