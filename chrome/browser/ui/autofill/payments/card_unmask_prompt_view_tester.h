// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_TESTER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_TESTER_H_

#include <memory>

namespace autofill {

class CardUnmaskPromptView;

// Functionality that helps to test an AutofillCardUnmaskPromptView.
class CardUnmaskPromptViewTester {
 public:
  // Gets a AutofillCardUnmaskPromptViewTester for |view|.
  static std::unique_ptr<CardUnmaskPromptViewTester> For(
      CardUnmaskPromptView* view);

  virtual ~CardUnmaskPromptViewTester() {}

  virtual void Close() = 0;

  // Will enter a CVC value and click "Confirm" to advance to the next step.
  virtual void EnterCVCAndAccept() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_TESTER_H_
