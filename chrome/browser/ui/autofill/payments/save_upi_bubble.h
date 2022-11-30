// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_H_

namespace autofill {

// The desktop UI interface which displays the "Remember your UPI ID?" bubble.
// This object is responsible for its own lifetime.
class SaveUPIBubble {
 public:
  // Called to close the bubble and prevent future callbacks into the
  // controller.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_H_
