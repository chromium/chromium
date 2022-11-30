// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_BASE_H_

namespace autofill {

// TODO(@vishwasuppoor): Rename to be platform-agnostic (crbug.com/1322580).
// The cross-platform interface which displays the bubble for autofill bubbles.
// This object is responsible for its own lifetime.
class AutofillBubbleBase {
 public:
  // Called from controller to shut down the bubble and prevent any further
  // action.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_BASE_H_
