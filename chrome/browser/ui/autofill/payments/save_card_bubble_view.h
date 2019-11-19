// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEW_H_

#include "base/macros.h"

namespace autofill {

// The cross-platform UI interface which displays the "Save credit card?"
// bubble. This object is responsible for its own lifetime.
class SaveCardBubbleView {
 public:
  // Called to close the bubble and prevent future callbacks into the
  // controller.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEW_H_
