// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_

namespace autofill {

// This class serves as the base for all bubble controllers, which manage the
// logic and state of an Autofill bubble.
class BubbleControllerBase {
 public:
  virtual ~BubbleControllerBase() = default;

  // Instructs the controller to show the bubble view.
  virtual void ShowBubble() = 0;

  // Instructs the controller to hide the bubble view.
  virtual void HideBubble() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_CONTROLLER_BASE_H_
