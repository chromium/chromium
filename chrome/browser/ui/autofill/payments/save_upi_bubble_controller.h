// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_CONTROLLER_H_

#include <string>


namespace autofill {

// Interface that exposes controller functionality to SaveUPIBubbleView.
class SaveUPIBubbleController {
 public:
  SaveUPIBubbleController() = default;
  virtual ~SaveUPIBubbleController() = default;

  // Returns the UPI ID being stored.
  virtual std::u16string GetUpiId() const = 0;

  // The user accepted the prompt to save the UPI ID.
  virtual void OnAccept() = 0;

  virtual void OnBubbleClosed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_CONTROLLER_H_
